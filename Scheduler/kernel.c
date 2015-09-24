#include <errno.h>
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

#include "kernel.h"
#include "pgbgw.h"
#include "report.h"

int
revisionSessionsForDeviceDisplacementInSight(int *busy)
{
	StringInfoData  infoData;
    TupleDesc       tupdesc;
	HeapTuple       tuple;
	bool            isNull;
	uint32          numberOfProcessedSessions;
	int             rc;

	PGBGW_BEGIN;

	initStringInfo(&infoData);
	appendStringInfo(
		&infoData, "\
SELECT journal.revision_sessions_for_device_displacement_in_sight()"
	);

	SetCurrentStatementStartTimestamp();
    rc = SPI_exec(infoData.data, 0);
	if (rc != SPI_OK_SELECT) {
		reportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

    if (SPI_processed != 1) {
	    reportError("Unexpected number of tuples");
   	    PGBGW_ROLLBACK;
	    return -1;
    }

    tupdesc = SPI_tuptable->tupdesc;
    tuple = SPI_tuptable->vals[0];

    numberOfProcessedSessions = DatumGetUInt32(SPI_getbinval(tuple, tupdesc, 1, &isNull));

    PGBGW_COMMIT;

    *busy += numberOfProcessedSessions;

	return 0;
}

int
revisionSessionsForDeviceDisplacementOnMap(int *busy)
{
	StringInfoData  infoData;
    TupleDesc       tupdesc;
	HeapTuple       tuple;
	bool            isNull;
	uint32          numberOfProcessedSessions;
	int             rc;

	PGBGW_BEGIN;

	initStringInfo(&infoData);
	appendStringInfo(
		&infoData, "\
SELECT journal.revision_sessions_for_device_displacement_on_map()"
	);

	SetCurrentStatementStartTimestamp();
    rc = SPI_exec(infoData.data, 0);
	if (rc != SPI_OK_SELECT) {
		reportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

    if (SPI_processed != 1) {
	    reportError("Unexpected number of tuples");
   	    PGBGW_ROLLBACK;
	    return -1;
    }

    tupdesc = SPI_tuptable->tupdesc;
    tuple = SPI_tuptable->vals[0];

    numberOfProcessedSessions = DatumGetUInt32(SPI_getbinval(tuple, tupdesc, 1, &isNull));

    PGBGW_COMMIT;

    *busy += numberOfProcessedSessions;

	return 0;
}

int
revisionSessionsForModifiedPlaques(int *busy)
{
	StringInfoData  infoData;
    TupleDesc       tupdesc;
	HeapTuple       tuple;
	bool            isNull;
	uint32          numberOfProcessedSessions;
	int             rc;

	PGBGW_BEGIN;

	initStringInfo(&infoData);
	appendStringInfo(
		&infoData, "\
SELECT journal.revision_sessions_for_modified_plaques()"
	);

	SetCurrentStatementStartTimestamp();
    rc = SPI_exec(infoData.data, 0);
	if (rc != SPI_OK_SELECT) {
		reportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

    if (SPI_processed != 1) {
	    reportError("Unexpected number of tuples");
   	    PGBGW_ROLLBACK;
	    return -1;
    }

    tupdesc = SPI_tuptable->tupdesc;
    tuple = SPI_tuptable->vals[0];

    numberOfProcessedSessions = DatumGetUInt32(SPI_getbinval(tuple, tupdesc, 1, &isNull));

    PGBGW_COMMIT;

    *busy += numberOfProcessedSessions;

	return 0;
}
