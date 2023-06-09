#include <errno.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <postgres.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/proc.h>
#include <executor/spi.h>
#include <fmgr.h>

#include "broadcaster_api.h"
#include "desk.h"
#include "kernel.h"
#include "listener.h"
#include "report.h"

#define LATCH_TIMEOUT_IDLE 1000L
#define LATCH_TIMEOUT_BUSY 500L

/* Essential for shared libraries. */
PG_MODULE_MAGIC;

/* flags set by signal handlers */
static volatile sig_atomic_t gotSigHup = false;
static volatile sig_atomic_t gotSigTerm = false;

/* Entry point. */
void
_PG_init(void);

static void
broadcasterMain(Datum *arg);

static void
sigTermHandler(SIGNAL_ARGS);

static void
sigHupHandler(SIGNAL_ARGS);

static struct desk *
initDesk(void);

static void
cleanupDesk(struct desk *desk);

static int
startListener(struct desk *desk);

static int
stopListener(struct desk *desk);

void
_PG_init(void)
{
    BackgroundWorker worker;

    snprintf(worker.bgw_name, BGW_MAXLEN, "vp_broadcaster");
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 5;
    worker.bgw_main = (bgworker_main_type)broadcasterMain;
    worker.bgw_main_arg = (Datum)0;
#if PG_VERSION_NUM >= 90400
	worker.bgw_notify_pid = 0;
#endif

    RegisterBackgroundWorker(&worker);
}

static void
broadcasterMain(Datum *arg)
{
    static Latch    mainLatch;
    long            latchTimeout;
    int             latchStatus;
    struct desk     *desk;
	uint64          numberOfSessions;

	if (PQisthreadsafe() != 1)
		proc_exit(-1);

    InitializeLatchSupport();
    InitLatch(&mainLatch);

    pqsignal(SIGTERM, sigTermHandler);
    pqsignal(SIGHUP, sigHupHandler);

    BackgroundWorkerUnblockSignals();

    BackgroundWorkerInitializeConnection("vp", "vp");

	desk = initDesk();
	if (desk == NULL)
        proc_exit(-1);

    ReportInfo("Broadcaster ready");

    desk->listener.portNumber = BROADCASTER_PORT_NUMBER;

    if (startListener(desk) != 0)
        proc_exit(-1);

	while (!gotSigTerm)
	{
	    if (desk->watchdog.numberOfSessions == 0) {
    	    numberOfSessions = numberOfRevisedSessions();

	        if (numberOfSessions > 0) {
                getListOfRevisedSessions(desk);
                listenerKnockKnock(desk);
            }

            latchTimeout = (numberOfSessions == 0) ? LATCH_TIMEOUT_IDLE : LATCH_TIMEOUT_BUSY;
        } else {
            listenerKnockKnock(desk);

            latchTimeout = LATCH_TIMEOUT_BUSY;
        }

   		latchStatus = WaitLatch(&MyProc->procLatch,
            WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
            latchTimeout);
    	ResetLatch(&MyProc->procLatch);

    	// Emergency bailout if postmaster has died.
   		//
    	if (latchStatus & WL_POSTMASTER_DEATH)
            break;

		// In case of a SIGHUP, just reload the configuration.
		//
		if (gotSigHup)
            gotSigHup = false;
    }

    stopListener(desk);

    cleanupDesk(desk);

	proc_exit(0);
}

static void
sigTermHandler(SIGNAL_ARGS)
{
    int savedErrno = errno;

    gotSigTerm = true;

    if (MyProc)
        SetLatch(&MyProc->procLatch);

    errno = savedErrno;
}

static void
sigHupHandler(SIGNAL_ARGS)
{
    gotSigHup = true;

    if (MyProc)
        SetLatch(&MyProc->procLatch);
}

static struct desk *
initDesk(void)
{
	struct desk *desk;
    bool found;
	pthread_mutexattr_t mutexAttr;
	int rc;

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
	desk = ShmemInitStruct("vpBroadcasterDesk", sizeof(struct desk), &found);
	LWLockRelease(AddinShmemInitLock);

	if (desk == NULL) {
        ReportError("Cannot get shared memory");
        return NULL;
    }

    pthread_mutexattr_init(&mutexAttr);
    //pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);

    rc = pthread_mutex_init(&desk->listener.readyToGoMutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return NULL;
    }

    rc = pthread_cond_init(&desk->listener.readyToGoCond, NULL);
	if (rc != 0) {
		ReportError("Cannot initialize condition: rc=%d", rc);
        return NULL;
    }

	rc = pthread_mutex_init(&desk->watchdog.mutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return NULL;
    }

	desk->watchdog.lastReceiptId = 0;

    return desk;
}

static void
cleanupDesk(struct desk *desk)
{
}

static int
startListener(struct desk *desk)
{
    int rc;

    // For portability, explicitly create threads in a joinable state.
    //
    pthread_attr_init(&desk->listener.attributes);
    pthread_attr_setdetachstate(&desk->listener.attributes, PTHREAD_CREATE_JOINABLE);

	rc = pthread_create(&desk->listener.thread, &desk->listener.attributes, &listenerThread, desk);
    if (rc != 0) {
        ReportError("Cannot create listener thread: errno=%d", errno);
        return -1;
    }

    return 0;
}

static int
stopListener(struct desk *desk)
{
    int rc;

    rc = pthread_cancel(desk->listener.thread);
    if ((rc != 0) && (rc != ESRCH)) {
        ReportError("Cannot cancel listener thread: rc=%d", rc);
        return -1;
    }

    rc = pthread_attr_destroy(&desk->listener.attributes);
    if (rc != 0) {
        ReportError("Cannot destroy thread attributes: rc=%d", rc);
        return -1;
    }

    return 0;
}
