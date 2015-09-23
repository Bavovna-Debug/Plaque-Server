#include <errno.h>
#include <libpq-fe.h>
#include <string.h>
#include <unistd.h>

#include <postgres.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/proc.h>
#include <executor/spi.h>
#include <fmgr.h>

#include "kernel.h"
#include "report.h"

#define LATCH_TIMEOUT_WHEN_IDLE 1000
#define LATCH_TIMEOUT_WHEN_BUSY 100

/* Essential for shared libraries. */
PG_MODULE_MAGIC;

/* flags set by signal handlers */
static volatile sig_atomic_t gotSigHup = false;
static volatile sig_atomic_t gotSigTerm = false;

/* Entry point. */
void
_PG_init(void);

static void
schedulerMain(Datum *arg);

static void
sigTermHandler(SIGNAL_ARGS);

static void
sigHupHandler(SIGNAL_ARGS);

void
_PG_init(void)
{
    BackgroundWorker worker;

    snprintf(worker.bgw_name, BGW_MAXLEN, "vp_scheduler");
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 5;
    worker.bgw_main = (bgworker_main_type)schedulerMain;
    worker.bgw_main_arg = (Datum)0;
#if PG_VERSION_NUM >= 90400
	worker.bgw_notify_pid = 0;
#endif

    RegisterBackgroundWorker(&worker);
}

static void
schedulerMain(Datum *arg)
{
    static Latch mainLatch;
    int latchStatus;
    int latchTimeout;
    int numberOfProcessedSessions;

	if (PQisthreadsafe() != 1)
		proc_exit(-1);

    InitializeLatchSupport();
    InitLatch(&mainLatch);

    pqsignal(SIGTERM, sigTermHandler);
    pqsignal(SIGHUP, sigHupHandler);

    BackgroundWorkerUnblockSignals();

    BackgroundWorkerInitializeConnection("vp", "vp");

    reportLog("Scheduler ready");

	while (!gotSigTerm)
	{
	    numberOfProcessedSessions = 0;
	    revisionSessionsForDeviceDisplacement(&numberOfProcessedSessions);
	    revisionSessionsForModifiedPlaques(&numberOfProcessedSessions);

        if (numberOfProcessedSessions > 0)
    	    reportLog("Processed %d sessions", numberOfProcessedSessions);

        latchTimeout = (numberOfProcessedSessions) ? LATCH_TIMEOUT_WHEN_BUSY : LATCH_TIMEOUT_WHEN_IDLE;

   		latchStatus = WaitLatch(&MyProc->procLatch,
            WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
            latchTimeout);
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
