#include <c.h>
#include <string.h>

#include "api.h"
#include "chalkboard.h"
#include "db.h"
#include "tasks.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

int
GetSessionForDevice(
    struct Task *task,
    struct dbh *dbh,
    uint64 deviceId,
    uint64 *sessionId,
    char *knownSessionToken,
    char *givenSessionToken)
{
#define QUERY_GET_SESSION "\
SELECT session_id, session_token \
FROM journal.get_session($1, $2)"

    DB_PushBIGINT(dbh, &deviceId);
    DB_PushUUID(dbh, knownSessionToken);

	DB_Execute(dbh, QUERY_GET_SESSION);

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 2))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, INT8OID) ||
	    !DB_CorrectColumnType(dbh->result, 1, UUIDOID))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	memcpy(sessionId, PQgetvalue(dbh->result, 0, 0), sizeof(uint64));
	memcpy(givenSessionToken, PQgetvalue(dbh->result, 0, 1), API_TokenBinarySize);

	return 0;
}

int
SetAllSessionsOffline(void)
{
#define QUERY_SET_ALL_SESSIONS_OFFLINE "\
UPDATE journal.sessions \
SET satellite_task_id = NULL \
WHERE satellite_task_id IS NOT NULL"

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.auth);
	if (dbh == NULL)
		return -1;

	DB_Execute(dbh, QUERY_SET_ALL_SESSIONS_OFFLINE);

	if (!DB_CommandOK(dbh, dbh->result))
	{
		DB_PokeHandle(dbh);
		return -1;
	}

	DB_PokeHandle(dbh);

	return 0;
}

int
SetSessionOnline(struct Task *task)
{
#define QUERY_SET_SESSION_ONLINE "\
UPDATE journal.sessions \
SET satellite_task_id = $2 \
WHERE session_id = $1"

#define QUERY_SET_NEED_REVISION_ON "\
UPDATE journal.device_displacements \
SET need_on_radar_revision = TRUE, \
    need_in_sight_revision = TRUE \
WHERE device_id = $1"

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.auth);
	if (dbh == NULL)
	{
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 satelliteTaskId = htobe32(task->taskId);

    DB_PushBIGINT(dbh, &task->sessionId);
    DB_PushINTEGER(dbh, &satelliteTaskId);

	DB_Execute(dbh, QUERY_SET_SESSION_ONLINE);

	if (!DB_CommandOK(dbh, dbh->result))
	{
    	DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    DB_PushBIGINT(dbh, &task->deviceId);

	DB_Execute(dbh, QUERY_SET_NEED_REVISION_ON);

	if (!DB_CommandOK(dbh, dbh->result))
	{
    	DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	DB_PokeHandle(dbh);

	return 0;
}

int
SetSessionOffline(struct Task *task)
{
#define QUERY_SET_SESSION_OFFLINE "\
UPDATE journal.sessions \
SET satellite_task_id = NULL \
WHERE session_id = $1"

#define QUERY_SET_NEED_REVISION_OFF "\
UPDATE journal.device_displacements \
SET need_on_radar_revision = FALSE, \
    need_in_sight_revision = FALSE \
WHERE device_id = $1"

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.auth);
	if (dbh == NULL)
	{
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    DB_PushBIGINT(dbh, &task->sessionId);

	DB_Execute(dbh, QUERY_SET_SESSION_OFFLINE);

	if (!DB_CommandOK(dbh, dbh->result))
	{
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    DB_PushBIGINT(dbh, &task->deviceId);

	DB_Execute(dbh, QUERY_SET_NEED_REVISION_OFF);

	if (!DB_CommandOK(dbh, dbh->result))
	{
    	DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	DB_PokeHandle(dbh);

	return 0;
}

#define QUERY_GET_SESSION_REVISIONS "\
SELECT on_radar_revision, in_sight_revision, on_map_revision \
FROM journal.sessions \
WHERE session_id = $1"

#define QUERY_GET_ON_RADAR_REVISION "\
SELECT in_sight_revision \
FROM journal.sessions \
WHERE session_id = $1"

#define QUERY_GET_IN_SIGHT_REVISION "\
SELECT in_sight_revision \
FROM journal.sessions \
WHERE session_id = $1"

#define QUERY_GET_ON_MAP_REVISION "\
SELECT on_map_revision \
FROM journal.sessions \
WHERE session_id = $1"

int
GetSessionRevisions(
    struct Task         *task,
    struct Revisions    *revisions)
{
	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL)
	{
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    DB_PushBIGINT(dbh, &task->sessionId);

	DB_Execute(dbh, QUERY_GET_SESSION_REVISIONS);

	if (!DB_TuplesOK(dbh, dbh->result))
	{
        DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 3))
	{
        DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
        DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, INT4OID))
	{
        DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 1, INT4OID))
	{
        DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 2, INT4OID))
	{
        DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    revisions->onRadar = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 0));
    revisions->inSight = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 1));
    revisions->onMap = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 2));

    DB_PokeHandle(dbh);

	return 0;
}

int
GetSessionOnRadarRevision(
    struct Task *task,
    struct dbh  *dbh,
    uint32      *revision)
{
    DB_PushBIGINT(dbh, &task->sessionId);

	DB_Execute(dbh, QUERY_GET_ON_RADAR_REVISION);

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, INT4OID))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    *revision = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 0));

	return 0;
}

int
GetSessionInSightRevision(
    struct Task *task,
    struct dbh  *dbh,
    uint32      *revision)
{
    DB_PushBIGINT(dbh, &task->sessionId);

	DB_Execute(dbh, QUERY_GET_IN_SIGHT_REVISION);

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, INT4OID))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    *revision = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 0));

	return 0;
}

int
GetSessionOnMapRevision(
    struct Task *task,
    struct dbh  *dbh,
    uint32      *revision)
{
    DB_PushBIGINT(dbh, &task->sessionId);

	DB_Execute(dbh, QUERY_GET_ON_MAP_REVISION);

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, INT4OID))
	{
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    *revision = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 0));

	return 0;
}
