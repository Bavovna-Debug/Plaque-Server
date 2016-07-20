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
#include "apns_thread.h"
#include "chalkboard.h"
#include "mmps.h"
#include "notification.h"
#include "report.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

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

void
_PG_init(void)
{
    BackgroundWorker worker;

    snprintf(worker.bgw_name, BGW_MAXLEN, "vp_messanger");
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 5;
    worker.bgw_main = (bgworker_main_type) messangerMain;
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
    int             rc;

	if (PQisthreadsafe() != 1)
		proc_exit(-1);

    InitializeLatchSupport();
    InitLatch(&mainLatch);

    pqsignal(SIGTERM, sigTermHandler);
    pqsignal(SIGHUP, sigHupHandler);

    BackgroundWorkerUnblockSignals();

    BackgroundWorkerInitializeConnection("vp", "vp");

    // Create chalkboard - storage for all global variables.
    //
    rc = CreateChalkboard();
    if (rc != 0)
        exit(EXIT_FAILURE);

    ReportInfo("Messanger ready");

    rc = APNS_Start();
    if (rc != 0)
        proc_exit(-1);

    ResetInMessangerFlag();

	while (!gotSigTerm)
	{
	    if (NumberOfOutstandingNotifications() > 0)
	    {
            FetchListOfOutstandingNotifications();
            FetchNotificationsToMessanger();
            MoveOutstandingToInTheAir();
            KnockKnock();
        }

        FlagSentNotification();
        ReleaseProcessedNotificationsFromMessanger();

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

    APNS_Stop();

    DestroyChalkboard();

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
