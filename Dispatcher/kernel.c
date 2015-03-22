#include <errno.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <string.h>
#include <postgres.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/proc.h>
#include <fmgr.h>
#include "listener.h"

/* Essential for shared libraries. */
PG_MODULE_MAGIC;

/* Entry point. */
void
_PG_init(void);

static void
dispatcherMain(Datum *arg);

void
sigtermHandler(SIGNAL_ARGS);

void
sighupHandler(SIGNAL_ARGS);

void
_PG_init(void)
{
    BackgroundWorker worker;

    snprintf(worker.bgw_name, BGW_MAXLEN, "vp_dispatcher");
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 5;
    worker.bgw_main = (bgworker_main_type)dispatcherMain;
    worker.bgw_main_arg = (Datum) 0;
    worker.bgw_notify_pid = 0;

    RegisterBackgroundWorker(&worker);
}

static void
dispatcherMain(Datum *arg)
{
    static Latch mainLatch;
	struct listenerArguments arguments;
	pthread_t listenerHandler;
	int rc;

	if (PQisthreadsafe() != 1)
		goto quit;

    InitializeLatchSupport();
    InitLatch(&mainLatch);

    pqsignal(SIGTERM, sigtermHandler);
    pqsignal(SIGHUP, sighupHandler);

    BackgroundWorkerUnblockSignals();

    BackgroundWorkerInitializeConnection("vp", "vp");

    constructDialogues();

    elog(LOG, "Dispatcher started");

	arguments.portNumber = 12000;

	rc = pthread_create(&listenerHandler, NULL, &listenerThread, (void *)&arguments);
    if (rc != 0) {
#ifdef DEBUG
        fprintf(stderr, "Can't create listener thread: %d (%s)\n", errno, strerror(rc));
#endif
        goto quit;
    }

    WaitLatch(&MyProc->procLatch,
        WL_LATCH_SET | WL_POSTMASTER_DEATH,
        0L);
    ResetLatch(&MyProc->procLatch);
    elog(LOG, "Dispatcher to exit");

	rc = pthread_join(listenerHandler, NULL);
    if (rc != 0) {
#ifdef DEBUG
        fprintf(stderr, "Error has occurred while waiting for listener thread: %d (%s)\n", errno, strerror(rc));
#endif
        goto quit;
    }

quit:

    destructDialogues();

    proc_exit(0);
}

void
sigtermHandler(SIGNAL_ARGS)
{
    int savedErrno = errno;
    if (MyProc)
        SetLatch(&MyProc->procLatch);
    errno = savedErrno;
}

void
sighupHandler(SIGNAL_ARGS)
{
    if (MyProc)
        SetLatch(&MyProc->procLatch);
}
