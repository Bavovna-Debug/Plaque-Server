#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include <postgres.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/lwlock.h>
#include <storage/proc.h>
#include <storage/shmem.h>
#include <access/xact.h>
#include <executor/spi.h>
#include <fmgr.h>
#include <lib/stringinfo.h>
#include <pgstat.h>
#include <utils/builtins.h>
#include <utils/snapmgr.h>
#include <tcop/utility.h>

#include "broadcaster_api.h"
#include "desk.h"
#include "kernel.h"
#include "pgbgw.h"
#include "report.h"

int
numberOfRevisedSessions(void)
{
	StringInfoData  infoData;
    TupleDesc       tupdesc;
	HeapTuple       tuple;
	bool            isNull;
	uint64          numberOfSessions;
	int             rc;

	PGBGW_BEGIN;

	initStringInfo(&infoData);
	appendStringInfo(
		&infoData, "\
SELECT COUNT(*) \
FROM journal.revised_sessions"
	);

    rc = SPI_exec(infoData.data, 1);
	if (rc != SPI_OK_SELECT) {
		reportError("Cannot execute statement, rc=%d", rc);

   	    PGBGW_ROLLBACK;

		return -1;
    }

	if (SPI_processed != 1) {
		reportError("Unexpected result");

   	    PGBGW_ROLLBACK;

		return -1;
    }

    tupdesc = SPI_tuptable->tupdesc;
    tuple = SPI_tuptable->vals[0];
    numberOfSessions = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isNull));

    if (numberOfSessions > 0)
        reportLog("There are %lu revised sessions", numberOfSessions);

    PGBGW_COMMIT;

	return numberOfSessions;
}

int
getListOfRevisedSessions(struct desk *desk)
{
	StringInfoData  infoData;
    TupleDesc       tupdesc;
	HeapTuple       tuple;
	bool            isNull;
	int             numberOfSessions;
	int             sessionNumber;
	struct session  *session;
	int             rc;

    if (pthread_spin_trylock(&desk->watchdog.lock) != 0) {
        reportLog("Broadcaster is still busy with xmit, wait for %d seconds",
            SLEEP_WHEN_LISTENER_IS_BUSY);
        sleep(SLEEP_WHEN_LISTENER_IS_BUSY);
        return 0;
    }

	PGBGW_BEGIN;

	initStringInfo(&infoData);
	appendStringInfo(
		&infoData, "\
DELETE FROM journal.revised_sessions \
RETURNING session_id"
	);

    rc = SPI_exec(infoData.data, MAX_REVISED_SESSIONS_PER_STEP);
	if (rc != SPI_OK_DELETE_RETURNING) {
        pthread_spin_unlock(&desk->watchdog.lock);

		reportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

	numberOfSessions = SPI_processed;

	if (SPI_processed > 0) {
        tupdesc = SPI_tuptable->tupdesc;

	    for (sessionNumber = 0; sessionNumber < numberOfSessions; sessionNumber++)
	    {
        	tuple = SPI_tuptable->vals[sessionNumber];

            desk->watchdog.lastReceiptId++;

	        session = &desk->watchdog.sessions[sessionNumber];

            session->receiptId = desk->watchdog.lastReceiptId;
            session->sessionId = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isNull));

            reportLog("Broadcaster has detected revised session %lu", session->sessionId);
        }

	    for (sessionNumber = 0; sessionNumber < numberOfSessions; sessionNumber++)
	    {
	        session = &desk->watchdog.sessions[sessionNumber];

    	    initStringInfo(&infoData);
        	appendStringInfo(
		        &infoData, "\
UPDATE journal.sessions \
SET on_radar_revised = FALSE, \
    in_sight_revised = FALSE \
WHERE session_id = %lu \
RETURNING in_cache_revision, on_radar_revision, in_sight_revision",
                session->sessionId
    	    );

            rc = SPI_exec(infoData.data, 1);
	        if (rc != SPI_OK_UPDATE_RETURNING) {
                pthread_spin_unlock(&desk->watchdog.lock);

    	    	reportError("Cannot execute statement, rc=%d", rc);
   	            PGBGW_ROLLBACK;
    	    	return -1;
            }

            if (SPI_processed != 1) {
                pthread_spin_unlock(&desk->watchdog.lock);

		        reportError("Unexpected number of tuples");
   	            PGBGW_ROLLBACK;
    	    	return -1;
            }

            tupdesc = SPI_tuptable->tupdesc;
        	tuple = SPI_tuptable->vals[0];

            session->inCacheRevision = DatumGetInt32(SPI_getbinval(tuple, tupdesc, 1, &isNull));
            session->onRadarRevision = DatumGetInt32(SPI_getbinval(tuple, tupdesc, 2, &isNull));
            session->inSightRevision = DatumGetInt32(SPI_getbinval(tuple, tupdesc, 3, &isNull));
        }

        desk->watchdog.numberOfSessions = numberOfSessions;
	}

    PGBGW_COMMIT;

    pthread_spin_unlock(&desk->watchdog.lock);

	return 0;
}
