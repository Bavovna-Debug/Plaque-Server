#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <syslog.h>

#include "chalkboard.h"
#include "paquet.h"
#include "report.h"

// Pointer to the chalkboard. All modules, which need access to the chalkboard,
// should declare chalkboard as external variable.
//
struct Chalkboard *chalkboard = NULL;

static int
InitializeMMPS(void);

static int
InitializeListener(void);

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
    int                     rc;

    chalkboard = malloc(sizeof(struct Chalkboard));
    if (chalkboard == NULL)
    {
        ReportSoftAlert("Out of memory");

        return -1;
    }

    bzero((void *) chalkboard, sizeof(struct Chalkboard));

    rc = InitializeMMPS();
    if (rc != 0)
        return -1;

    rc = InitializeListener();
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

	chalkboard->pools.task = MMPS_InitPool(1);
	chalkboard->pools.paquet = MMPS_InitPool(1);
	chalkboard->pools.dynamic = MMPS_InitPool(5);

	rc = MMPS_InitBank(chalkboard->pools.task, 0,
		sizeof(struct Task),
		0,
		NUMBER_OF_BUFFERS_TASK);
	if (rc != 0) {
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return -1;
    }

	rc = MMPS_InitBank(chalkboard->pools.paquet, 0,
		sizeof(struct Paquet),
		0,
		NUMBER_OF_BUFFERS_PAQUET);
	if (rc != 0) {
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return -1;
    }

	rc = MMPS_InitBank(chalkboard->pools.dynamic, 0,
		256,
		sizeof(struct PaquetPilot),
		NUMBER_OF_BUFFERS_256);
	if (rc != 0) {
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return -1;
    }

	rc = MMPS_InitBank(chalkboard->pools.dynamic, 1,
		512,
		sizeof(struct PaquetPilot),
		NUMBER_OF_BUFFERS_512);
	if (rc != 0) {
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return -1;
    }

	rc = MMPS_InitBank(chalkboard->pools.dynamic, 2,
		KB,
		sizeof(struct PaquetPilot),
		NUMBER_OF_BUFFERS_1K);
	if (rc != 0) {
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return -1;
    }

	rc = MMPS_InitBank(chalkboard->pools.dynamic, 3,
		4 * KB,
		sizeof(struct PaquetPilot),
		NUMBER_OF_BUFFERS_4K);
	if (rc != 0) {
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return -1;
    }

	rc = MMPS_InitBank(chalkboard->pools.dynamic, 4,
		MB,
		sizeof(struct PaquetPilot),
		NUMBER_OF_BUFFERS_1M);
	if (rc != 0) {
		ReportError("Cannot create buffer bank: rc=%d", rc);
        return -1;
    }

    return 0;
}

/**
 * InitializeListener()
 * Initialize network block of chalkboard.
 *
 * Returns 0 upon successful completion or an error code (negative value),
 * in case of error.
 */
static int
InitializeListener(void)
{
    chalkboard->listener.listenSockFD = 0;

    return 0;
}
