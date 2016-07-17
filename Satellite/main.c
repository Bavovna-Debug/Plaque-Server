#include <errno.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "broadcaster_api.h"
#include "broadcaster.h"
#include "db.h"
#include "desk.h"
#include "listener.h"
#include "mmps.h"
#include "paquet.h"
#include "report.h"
#include "session.h"
#include "tasks.h"
#include "task_list.h"

//volatile sig_atomic_t mustExit = 0;

#ifdef STATISTICS
static pthread_t statisticsHandler;
#endif
static pthread_t listenerHandler;
static pthread_t broadcasterHandler;

void
registerSignalHandler(void);

struct desk *
initDesk(void);

void
constructDB(struct desk *desk);

void
destructDB(struct desk *desk);

#ifdef STATISTICS
void *
statisticsThread(void *arg)
{
	struct desk *desk = (struct desk *)arg;

	while (1)
	{
		reportDebug("STATISTICS  DBH: %-4d%-4d%-4d  TASK: %-4d  PAQUET: %-4d  256: %-4d  512: %-4d  1K: %-4d  4K: %-4d  1M: %-4d",
			DB_HanldesInUse(desk->db.guardian),
			DB_HanldesInUse(desk->db.auth),
			DB_HanldesInUse(desk->db.plaque),
			MMPS_NumberOfBuffersInUse(desk->pools.task, 0),
			MMPS_NumberOfBuffersInUse(desk->pools.paquet, 0),
			MMPS_NumberOfBuffersInUse(desk->pools.dynamic, 0),
			MMPS_NumberOfBuffersInUse(desk->pools.dynamic, 1),
			MMPS_NumberOfBuffersInUse(desk->pools.dynamic, 2),
			MMPS_NumberOfBuffersInUse(desk->pools.dynamic, 3),
			MMPS_NumberOfBuffersInUse(desk->pools.dynamic, 4));
		sleep(1);
	}
	pthread_exit(NULL);
}
#endif

int
main(int argc, char *argv[])
{
	struct desk *desk;
	int rc;

	if (PQisthreadsafe() != 1)
		exit(-1);

	desk = initDesk();
    if (desk == NULL)
		exit(-1);

    registerSignalHandler();

	desk->listener.portNumber = atoi(argv[1]);

	desk->broadcaster.portNumber = BROADCASTER_PORT_NUMBER;

/*
	pthread_attr_t attr;
	rc = pthread_attr_init(&attr);
	if (rc != 0)
		reportError("pthread_attr_init: %d", rc);

	int stackSize = 0x800000;
	rc = pthread_attr_setstacksize(&attr, stackSize);
	if (rc != 0)
		reportError("pthread_attr_setstacksize: %d", rc);
*/

	constructDB(desk);

	if (setAllSessionsOffline(desk) != 0)
		exit(-1);

#ifdef STATISTICS
	rc = pthread_create(&statisticsHandler, NULL, &statisticsThread, desk);
    if (rc != 0) {
        reportError("Cannot create statistics thread: errno=%d", errno);
        goto quit;
    }
#endif

	rc = pthread_create(&listenerHandler, NULL, &listenerThread, desk);
    if (rc != 0) {
        reportError("Cannot create listener thread: errno=%d", errno);
        goto quit;
    }

	rc = pthread_create(&broadcasterHandler, NULL, &broadcasterThread, desk);
    if (rc != 0) {
        reportError("Cannot create broadcaster thread: errno=%d", errno);
        goto quit;
    }

	rc = pthread_join(listenerHandler, NULL);
    if (rc != 0) {
        reportError("Error has occurred while waiting for listener thread: errno=%d", errno);
        goto quit;
    }

quit:
	destructDB(desk);

	return 0;
}

void signalHandler(int signal)
{
	reportError("Received signal to quit: signal=%d", signal);

	pthread_kill(listenerHandler, signal);
}

void
registerSignalHandler(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = &signalHandler;

	sa.sa_flags = SA_RESETHAND;

	sigfillset(&sa.sa_mask);

    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

struct desk *
initDesk(void)
{
	struct desk	*desk;
	int			rc;

	desk = malloc(sizeof(struct desk));
	if (desk == NULL) {
        reportError("Out of memory");
        return NULL;
    }

	desk->pools.task = MMPS_InitPool(1);
	desk->pools.paquet = MMPS_InitPool(1);
	desk->pools.dynamic = MMPS_InitPool(5);

	rc = MMPS_InitBank(desk->pools.task, 0,
		sizeof(struct task),
		0,
		NUMBER_OF_BUFFERS_TASK);
	if (rc != 0) {
		reportError("Cannot create buffer bank: rc=%d", rc);
        return NULL;
    }

	rc = MMPS_InitBank(desk->pools.paquet, 0,
		sizeof(struct paquet),
		0,
		NUMBER_OF_BUFFERS_PAQUET);
	if (rc != 0) {
		reportError("Cannot create buffer bank: rc=%d", rc);
        return NULL;
    }

	rc = MMPS_InitBank(desk->pools.dynamic, 0,
		256,
		sizeof(struct paquetPilot),
		NUMBER_OF_BUFFERS_256);
	if (rc != 0) {
		reportError("Cannot create buffer bank: rc=%d", rc);
        return NULL;
    }

	rc = MMPS_InitBank(desk->pools.dynamic, 1,
		512,
		sizeof(struct paquetPilot),
		NUMBER_OF_BUFFERS_512);
	if (rc != 0) {
		reportError("Cannot create buffer bank: rc=%d", rc);
        return NULL;
    }

	rc = MMPS_InitBank(desk->pools.dynamic, 2,
		KB,
		sizeof(struct paquetPilot),
		NUMBER_OF_BUFFERS_1K);
	if (rc != 0) {
		reportError("Cannot create buffer bank: rc=%d", rc);
        return NULL;
    }

	rc = MMPS_InitBank(desk->pools.dynamic, 3,
		4 * KB,
		sizeof(struct paquetPilot),
		NUMBER_OF_BUFFERS_4K);
	if (rc != 0) {
		reportError("Cannot create buffer bank: rc=%d", rc);
        return NULL;
    }

	rc = MMPS_InitBank(desk->pools.dynamic, 4,
		MB,
		sizeof(struct paquetPilot),
		NUMBER_OF_BUFFERS_1M);
	if (rc != 0) {
		reportError("Cannot create buffer bank: rc=%d", rc);
        return NULL;
    }

	desk->tasks.list = initTaskList(desk);
	if (desk->tasks.list == NULL) {
        reportError("Cannot initialize task list");
        return NULL;
    }

    desk->listener.listenSockFD = 0;

	return desk;
}

void
constructDB(struct desk *desk)
{
	desk->db.guardian = DB_InitChain(
		"GUARDIAN",
		NUMBER_OF_DBH_GUARDIANS,
		"hostaddr = '127.0.0.1' dbname = 'guardian' user = 'guardian' password = 'nVUcDYDVZCMaRdCfayWrG23w'");

	desk->db.auth = DB_InitChain(
		"AUTH",
		NUMBER_OF_DBH_AUTHENTICATION,
		"hostaddr = '127.0.0.1' dbname = 'vp' user = 'vp' password = 'vi79HRhxbFahmCKFUKMAACrY'");

	desk->db.plaque = DB_InitChain(
		"PLAQUE",
		NUMBER_OF_DBH_PLAQUES_SESSION,
		"hostaddr = '127.0.0.1' dbname = 'vp' user = 'vp' password = 'vi79HRhxbFahmCKFUKMAACrY'");
}

void
destructDB(struct desk *desk)
{
	DB_ReleaseChain(desk->db.guardian);
	DB_ReleaseChain(desk->db.auth);
	DB_ReleaseChain(desk->db.plaque);
}
