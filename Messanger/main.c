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

#include "apns.h"
#include "desk.h"
#include "mmps.h"
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

static void
cleanupDesk(struct desk *desk);

static int
startAPNS(struct desk *desk);

static int
stopAPNS(struct desk *desk);

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

    ReportInfo("Messanger ready");

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

    	// Emergency bailout if postmaster has died.
   		//
    	if (latchStatus & WL_POSTMASTER_DEATH)
            break;

		// In case of a SIGHUP, just reload the configuration.
		//
		if (gotSigHup)
              gotSigHup = false;
    }

    stopAPNS(desk);

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
	struct desk         *desk;
    bool                found;
	pthread_mutexattr_t mutexAttr;
	int                 rc;

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
	desk = ShmemInitStruct("vpMessangerDesk", sizeof(struct desk), &found);
	LWLockRelease(AddinShmemInitLock);

	if (desk == NULL) {
        ReportError("Cannot get shared memory");
        return NULL;
    }

    pthread_mutexattr_init(&mutexAttr);
    //pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);

    rc = pthread_mutex_init(&desk->apns.readyToGoMutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return NULL;
    }

    rc = pthread_cond_init(&desk->apns.readyToGoCond, NULL);
	if (rc != 0) {
		ReportError("Cannot initialize condition: rc=%d", rc);
        return NULL;
    }

	desk->pools.notifications = MMPS_InitPool(1);
	if (desk->pools.notifications == NULL) {
		ReportError("Cannot create buffer pool");
        return NULL;
    }

	rc = MMPS_InitBank(desk->pools.notifications, 0,
		sizeof(struct notification),
		0,
		POOL_NOTIFICATIONS_NUMBER_OF_BUFFERS);
	if (rc != 0) {
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return NULL;
    }

	desk->pools.apns = MMPS_InitPool(1);
	if (desk->pools.apns == NULL) {
		ReportError("Cannot create buffer pool");
        return NULL;
    }

	rc = MMPS_InitBank(desk->pools.apns, 0,
		POOL_APNS_SIZE_OF_BUFFER,
		0,
		POOL_APNS_NUMBER_OF_BUFFERS);
	if (rc != 0) {
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return NULL;
    }

    desk->outstandingNotifications.buffers = NULL;
    desk->inTheAirNotifications.buffers = NULL;
    desk->sentNotifications.buffers = NULL;
    desk->processedNotifications.buffers = NULL;

	rc = pthread_mutex_init(&desk->outstandingNotifications.mutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return NULL;
    }

	rc = pthread_mutex_init(&desk->inTheAirNotifications.mutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return NULL;
    }

	rc = pthread_mutex_init(&desk->sentNotifications.mutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return NULL;
    }

	rc = pthread_mutex_init(&desk->processedNotifications.mutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return NULL;
    }

	return desk;
}

static void
cleanupDesk(struct desk *desk)
{
    int rc;

	rc = pthread_mutex_destroy(&desk->apns.readyToGoMutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_cond_destroy(&desk->apns.readyToGoCond);
	if (rc != 0)
		ReportError("Cannot destroy condition: rc=%d", rc);

	rc = pthread_mutex_destroy(&desk->outstandingNotifications.mutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_mutex_destroy(&desk->inTheAirNotifications.mutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_mutex_destroy(&desk->sentNotifications.mutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_mutex_destroy(&desk->processedNotifications.mutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);
}

static int
startAPNS(struct desk *desk)
{
    int rc;

    // For portability, explicitly create threads in a joinable state.
    //
    pthread_attr_init(&desk->apns.attributes);
    pthread_attr_setdetachstate(&desk->apns.attributes, PTHREAD_CREATE_JOINABLE);

    rc = pthread_create(&desk->apns.thread, &desk->apns.attributes, &apnsThread, desk);
    if (rc != 0) {
        ReportError("Cannot create APNS thread: %d", errno);
        return -1;
    }

    return 0;
}

static int
stopAPNS(struct desk *desk)
{
    int rc;

    rc = pthread_cancel(desk->apns.thread);
    if ((rc != 0) && (rc != ESRCH)) {
        ReportError("Cannot cancel APNS thread: rc=%d", rc);
        return -1;
    }

    rc = pthread_attr_destroy(&desk->apns.attributes);
    if (rc != 0) {
        ReportError("Cannot destroy thread attributes: rc=%d", rc);
        return -1;
    }

    return 0;
}
