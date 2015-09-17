#include <errno.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>

#include <postgres.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/proc.h>
#include <storage/spin.h>
#include <executor/spi.h>
#include <fmgr.h>

#include "apns.h"
#include "buffers.h"
#include "desk.h"
#include "notification.h"
#include "report.h"

#define LATCH_TIMEOUT 1000L

/* Essential for shared libraries. */
PG_MODULE_MAGIC;

/* flags set by signal handlers */
static volatile sig_atomic_t gotSigHup = false;
static volatile sig_atomic_t gotSigTerm = false;

/* Entry point. */
void
_PG_init(void);

static void
messangerMain(Datum *arg);

static void
sigTermHandler(SIGNAL_ARGS);

static void
sigHupHandler(SIGNAL_ARGS);

static struct desk *
initDesk(void);

static int
startAPNS(struct desk *desk);

void
_PG_init(void)
{
    BackgroundWorker worker;

    snprintf(worker.bgw_name, BGW_MAXLEN, "vp_messanger");
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 5;
    worker.bgw_main = (bgworker_main_type)messangerMain;
    worker.bgw_main_arg = (Datum)0;
#if PG_VERSION_NUM >= 90400
	worker.bgw_notify_pid = 0;
#endif

    RegisterBackgroundWorker(&worker);
}

static void
messangerMain(Datum *arg)
{
    static Latch    mainLatch;
    int             latchStatus;
    struct desk     *desk;

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

    reportLog("Messanger ready");

    if (startAPNS(desk) != 0)
        proc_exit(-1);

    resetInMessangerFlag();

	while (!gotSigTerm)
	{
	    if (numberOfOutstandingNotifications() > 0) {
            fetchListOfOutstandingNotifications(desk);
            fetchNotificationsToMessanger(desk);
            moveOutstandingToInTheAir(desk);
            apnsKnockKnock(desk);
        }

        flagSentNotification(desk);
        releaseProcessedNotificationsFromMessanger(desk);

   		latchStatus = WaitLatch(&MyProc->procLatch,
            WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
            LATCH_TIMEOUT);
    	ResetLatch(&MyProc->procLatch);

    	/*
	     * Emergency bailout if postmaster has died.
   		 */
    	if (latchStatus & WL_POSTMASTER_DEATH)
    	    break;

		/*
		 * In case of a SIGHUP, just reload the configuration.
		 */
		if (gotSigHup)
              gotSigHup = false;
    }

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
	//pthread_mutexattr_t mutexAttr;
	int rc;

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
	desk = ShmemInitStruct("vpMessangerDesk", sizeof(struct desk), &found);
	LWLockRelease(AddinShmemInitLock);

	if (desk == NULL) {
        reportError("Cannot get shared memory");
        return NULL;
    }

    //pthread_mutexattr_init(&mutexAttr);
    //pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);

    //rc = sem_init(&desk->apns.readyToGo, 1, 0);
    desk->apns.readyToGo = sem_open("vpMessangerAPNS", O_CREAT, 0644, 0);
    if (desk->apns.readyToGo == SEM_FAILED) {
        reportError("Cannot initialize semaphore: errno=%d", errno);
        return NULL;
    }

	desk->pools.notifications = initBufferPool(1);
	if (desk->pools.notifications == NULL) {
		reportError("Cannot create buffer pool");
        return NULL;
    }

	rc = initBufferChain(desk->pools.notifications, 0,
		sizeof(struct notification),
		0,
		POOL_NOTIFICATIONS_NUMBER_OF_BUFFERS);
	if (rc != 0) {
		reportError("Cannot create buffer chain: rc=%d", rc);
        return NULL;
    }

	desk->pools.apns = initBufferPool(1);
	if (desk->pools.apns == NULL) {
		reportError("Cannot create buffer pool");
        return NULL;
    }

	rc = initBufferChain(desk->pools.apns, 0,
		POOL_APNS_SIZE_OF_BUFFER,
		0,
		POOL_APNS_NUMBER_OF_BUFFERS);
	if (rc != 0) {
		reportError("Cannot create buffer chain: rc=%d", rc);
        return NULL;
    }

    desk->outstandingNotifications.buffers = NULL;
    desk->inTheAirNotifications.buffers = NULL;
    desk->sentNotifications.buffers = NULL;
    desk->processedNotifications.buffers = NULL;

	rc = pthread_spin_init(&desk->outstandingNotifications.lock, PTHREAD_PROCESS_SHARED);
	//rc = pthread_mutex_init(&desk->outstandingNotifications.lock, &mutexAttr);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return NULL;
    }

	rc = pthread_spin_init(&desk->inTheAirNotifications.lock, PTHREAD_PROCESS_SHARED);
	//rc = pthread_mutex_init(&desk->inTheAirNotifications.lock, &mutexAttr);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return NULL;
    }

	rc = pthread_spin_init(&desk->sentNotifications.lock, PTHREAD_PROCESS_SHARED);
	//rc = pthread_mutex_init(&desk->sentNotifications.lock, &mutexAttr);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return NULL;
    }

	rc = pthread_spin_init(&desk->processedNotifications.lock, PTHREAD_PROCESS_SHARED);
	//rc = pthread_mutex_init(&desk->processedNotifications.lock, &mutexAttr);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return NULL;
    }

	return desk;
}

static int
startAPNS(struct desk *desk)
{
/*
    pid_t pid;

    pid = fork();

    if (pid < 0) {
        reportError("Cannot create APN process: errno=%d", errno);
        return -1;
    } else if (pid == 0) {
        apnsThread(desk);
    }
*/
    int rc;

    rc = pthread_create(&desk->apns.thread, NULL, &apnsThread, desk);
    if (rc != 0) {
        reportError("Cannot create APN thread: %d", errno);
        return -1;
    }

    return 0;
}
