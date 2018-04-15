#include <errno.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

#include "broadcaster_api.h"
#include "broadcaster.h"
#include "chalkboard.h"
#include "db.h"
#include "listener.h"
#include "mmps.h"
#include "paquet.h"
#include "report.h"
#include "session.h"
#include "tasks.h"
#include "task_list.h"

// Chalkboard is kept in the chalkboard module and must be accessed
// as an external variable.
//
extern struct Chalkboard *chalkboard;

//volatile sig_atomic_t mustExit = 0;

#ifdef STATISTICS
static pthread_t statisticsHandler;
#endif

#ifdef IPV4
static pthread_t listenerIPv4Handler;
#endif

#ifdef IPV6
static pthread_t listenerIPv6Handler;
#endif

static pthread_t broadcasterHandler;

static void
RegisterSignalHandler(void);

static void
ConstructDB(void);

static void
DestructDB(void);

#ifdef STATISTICS
void *
StatisticsThread(void *arg)
{
	while (1)
	{
		ReportDebug("STATISTICS  DBH: %-4d%-4d%-4d  TASK: %-4d  PAQUET: %-4d  256: %-4d  512: %-4d  1K: %-4d  4K: %-4d  1M: %-4d",
			DB_HanldesInUse(chalkboard->db.guardian),
			DB_HanldesInUse(chalkboard->db.auth),
			DB_HanldesInUse(chalkboard->db.plaque),
			MMPS_NumberOfBuffersInUse(chalkboard->pools.task, 0),
			MMPS_NumberOfBuffersInUse(chalkboard->pools.paquet, 0),
			MMPS_NumberOfBuffersInUse(chalkboard->pools.dynamic, 0),
			MMPS_NumberOfBuffersInUse(chalkboard->pools.dynamic, 1),
			MMPS_NumberOfBuffersInUse(chalkboard->pools.dynamic, 2),
			MMPS_NumberOfBuffersInUse(chalkboard->pools.dynamic, 3),
			MMPS_NumberOfBuffersInUse(chalkboard->pools.dynamic, 4));
		sleep(1);
	}
	pthread_exit(NULL);
}
#endif

int
main(int argc, char *argv[])
{
	int rc;

	if (PQisthreadsafe() != 1)
		exit(-1);

#ifdef SYSLOG
	//
    // Prepare logging facilities.
    //
    openlog("vp", LOG_PID | LOG_CONS | LOG_NDELAY, LOG_DAEMON);
    //
    setlogmask(LOG_UPTO(LOG_DEBUG));
#endif

	rc = CreateChalkboard();
    if (rc != 0)
	{
        ReportError("Cannot initialize chalkboard");
		exit(EXIT_FAILURE);
    }

	rc = InitTaskList();
	if (rc != 0)
	{
        ReportError("Cannot initialize task list");
		exit(EXIT_FAILURE);
    }

    RegisterSignalHandler();

    chalkboard->listenerIPv4.portNumber = TCP_PortNumberIPv4;
    chalkboard->listenerIPv6.portNumber = TCP_PortNumberIPv6;

	chalkboard->broadcaster.portNumber = BROADCASTER_PORT_NUMBER;

/*
	pthread_attr_t attr;
	rc = pthread_attr_init(&attr);
	if (rc != 0)
		ReportError("pthread_attr_init: %d", rc);

	int stackSize = 0x800000;
	rc = pthread_attr_setstacksize(&attr, stackSize);
	if (rc != 0)
		ReportError("pthread_attr_setstacksize: %d", rc);
*/

	ConstructDB();

	rc = SetAllSessionsOffline();
	if (rc != 0)
		exit(-1);

#ifdef STATISTICS
	rc = pthread_create(&statisticsHandler, NULL, &StatisticsThread, NULL);
    if (rc != 0) {
        ReportError("Cannot create statistics thread: errno=%d", errno);
        goto quit;
    }
#endif

#ifdef IPV4
	rc = pthread_create(&listenerIPv4Handler, NULL, &IPv4ListenerThread, NULL);
    if (rc != 0) {
        ReportError("Cannot create IPv4 listener thread: errno=%d", errno);
        goto quit;
    }
#endif

#ifdef IPV6
    rc = pthread_create(&listenerIPv6Handler, NULL, &IPv6ListenerThread, NULL);
    if (rc != 0) {
        ReportError("Cannot create IPv6 listener thread: errno=%d", errno);
        goto quit;
    }
#endif

	rc = pthread_create(&broadcasterHandler, NULL, &BroadcasterThread, NULL);
    if (rc != 0) {
        ReportError("Cannot create broadcaster thread: errno=%d", errno);
        goto quit;
    }

#ifdef IPV4
	rc = pthread_join(listenerIPv4Handler, NULL);
    if (rc != 0) {
        ReportError("Error has occurred while waiting for IPv4 listener thread: errno=%d", errno);
        goto quit;
    }
#endif

#ifdef IPV6
    rc = pthread_join(listenerIPv6Handler, NULL);
    if (rc != 0) {
        ReportError("Error has occurred while waiting for IPv6 listener thread: errno=%d", errno);
        goto quit;
    }
#endif

    rc = pthread_join(broadcasterHandler, NULL);
    if (rc != 0) {
        ReportError("Error has occurred while waiting for broadcaster thread: errno=%d", errno);
        goto quit;
    }

quit:
	DestructDB();

    exit(EXIT_SUCCESS);
}

void
SignalHandler(int signal)
{
	ReportError("Received signal to quit: signal=%d", signal);

#ifdef IPV4
    pthread_kill(listenerIPv4Handler, signal);
#endif

#ifdef IPV6
    pthread_kill(listenerIPv6Handler, signal);
#endif
    pthread_kill(broadcasterHandler, signal);
}

static void
RegisterSignalHandler(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = &SignalHandler;

	sa.sa_flags = SA_RESETHAND;

	sigfillset(&sa.sa_mask);

    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

static void
ConstructDB(void)
{
	chalkboard->db.guardian = DB_InitChain(
		"GUARDIAN",
		NUMBER_OF_DBH_GUARDIANS,
		"hostaddr = '127.0.0.1' dbname = 'guardian' user = 'guardian' password = 'nVUcDYDVZCMaRdCfayWrG23w'");

	chalkboard->db.auth = DB_InitChain(
		"AUTH",
		NUMBER_OF_DBH_AUTHENTICATION,
		"hostaddr = '127.0.0.1' dbname = 'vp' user = 'vp' password = 'vi79HRhxbFahmCKFUKMAACrY'");

	chalkboard->db.plaque = DB_InitChain(
		"PLAQUE",
		NUMBER_OF_DBH_PLAQUES_SESSION,
		"hostaddr = '127.0.0.1' dbname = 'vp' user = 'vp' password = 'vi79HRhxbFahmCKFUKMAACrY'");
}

static void
DestructDB(void)
{
	DB_ReleaseChain(chalkboard->db.guardian);
	DB_ReleaseChain(chalkboard->db.auth);
	DB_ReleaseChain(chalkboard->db.plaque);
}
