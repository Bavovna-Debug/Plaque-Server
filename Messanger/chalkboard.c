#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <postgres.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/proc.h>
#include <executor/spi.h>
#include <fmgr.h>

#include "chalkboard.h"
#include "notification.h"
#include "report.h"

// Pointer to the chalkboard. All modules, which need access to the chalkboard,
// should declare chalkboard as external variable.
//
struct Chalkboard *chalkboard = NULL;

static int
InitializeMMPS(void);

static int
InitializeLocks(void);

/**
 * CreateChalkboard()
 * Allocate and initialize chalkboard.
 *
 * Returns 0 upon successful completion or an error code (negative value),
 * in case of error.
 */
int
CreateChalkboard(void)
{
    bool    found;
    int     rc;

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
	chalkboard = (struct Chalkboard *)
	    ShmemInitStruct("vpMessangerDesk", sizeof(struct Chalkboard), &found);
	LWLockRelease(AddinShmemInitLock);

    if (chalkboard == NULL)
    {
        ReportSoftAlert("Out of memory");

        return -1;
    }

    bzero((void *) chalkboard, sizeof(struct Chalkboard));

    rc = InitializeMMPS();
    if (rc != 0)
        return -1;

    rc = InitializeLocks();
    if (rc != 0)
        return -1;

    return 0;
}

/**
 * InitializeMMPS()
 * Initialize MMPS.
 *
 * Returns 0 upon successful completion or an error code (negative value),
 * in case of error.
 */
static int
InitializeMMPS(void)
{
	int rc;

	chalkboard->pools.notifications = MMPS_InitPool(1);
	if (chalkboard->pools.notifications == NULL)
	{
		ReportError("Cannot create buffer pool");
        return -1;
    }

	rc = MMPS_InitBank(chalkboard->pools.notifications, 0,
		sizeof(struct Notification),
		0,
		POOL_NOTIFICATIONS_NUMBER_OF_BUFFERS);
	if (rc != 0)
	{
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return -1;
    }

    MMPS_AllocateImmediately(chalkboard->pools.notifications, 0);

	chalkboard->pools.apns = MMPS_InitPool(1);
	if (chalkboard->pools.apns == NULL)
	{
		ReportError("Cannot create buffer pool");
        return -1;
    }

	rc = MMPS_InitBank(chalkboard->pools.apns, 0,
		POOL_APNS_SIZE_OF_BUFFER,
		0,
		POOL_APNS_NUMBER_OF_BUFFERS);
	if (rc != 0)
	{
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return -1;
    }

    MMPS_AllocateImmediately(chalkboard->pools.apns, 0);

    chalkboard->outstandingNotifications.buffers = NULL;
    chalkboard->inTheAirNotifications.buffers = NULL;
    chalkboard->sentNotifications.buffers = NULL;
    chalkboard->processedNotifications.buffers = NULL;

    return 0;
}

/**
 * InitializeLocks()
 * Initialize mutexes, spinlocks and conditions.
 *
 * Returns 0 upon successful completion or an error code (negative value),
 * in case of error.
 */
static int
InitializeLocks(void)
{
	pthread_mutexattr_t mutexAttr;
	int                 rc;

    pthread_mutexattr_init(&mutexAttr);
    //pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);

    rc = pthread_mutex_init(&chalkboard->apns.readyToGoMutex, &mutexAttr);
	if (rc != 0)
	{
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    rc = pthread_cond_init(&chalkboard->apns.readyToGoCond, NULL);
	if (rc != 0)
	{
		ReportError("Cannot initialize condition: rc=%d", rc);
        return -1;
    }

	rc = pthread_mutex_init(&chalkboard->outstandingNotifications.mutex, &mutexAttr);
	if (rc != 0)
	{
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

	rc = pthread_mutex_init(&chalkboard->inTheAirNotifications.mutex, &mutexAttr);
	if (rc != 0)
	{
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

	rc = pthread_mutex_init(&chalkboard->sentNotifications.mutex, &mutexAttr);
	if (rc != 0)
	{
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

	rc = pthread_mutex_init(&chalkboard->processedNotifications.mutex, &mutexAttr);
	if (rc != 0)
	{
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    return 0;
}

/**
 * DestroyChalkboard()
 * Release all resources.
 */
void
DestroyChalkboard(void)
{
    int rc;

	rc = pthread_mutex_destroy(&chalkboard->apns.readyToGoMutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_cond_destroy(&chalkboard->apns.readyToGoCond);
	if (rc != 0)
		ReportError("Cannot destroy condition: rc=%d", rc);

	rc = pthread_mutex_destroy(&chalkboard->outstandingNotifications.mutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_mutex_destroy(&chalkboard->inTheAirNotifications.mutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_mutex_destroy(&chalkboard->sentNotifications.mutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_mutex_destroy(&chalkboard->processedNotifications.mutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);
}
