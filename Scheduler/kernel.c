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
revisionSessionsForModifiedPlaques(void)
{
	StringInfoData  infoData;
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

    PGBGW_COMMIT;

	return 0;
}
