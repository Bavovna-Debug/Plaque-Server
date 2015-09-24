#include <c.h>
#include <string.h>

#include "api.h"
#include "db.h"
#include "tasks.h"

int
getSessionForDevice(
    struct task *task,
    struct dbh *dbh,
    uint64 deviceId,
    uint64 *sessionId,
    char *knownSessionToken,
    char *givenSessionToken)
{
    dbhPushBIGINT(dbh, &deviceId);
    dbhPushUUID(dbh, knownSessionToken);

	dbhExecute(dbh, "\
SELECT session_id, session_token \
FROM journal.get_session($1, $2)");

	if (!dbhTuplesOK(dbh, dbh->result)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 2)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, INT8OID) ||
	    !dbhCorrectColumnType(dbh->result, 1, UUIDOID)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	memcpy(sessionId, PQgetvalue(dbh->result, 0, 0), sizeof(uint64));
	memcpy(givenSessionToken, PQgetvalue(dbh->result, 0, 1), TokenBinarySize);

	return 0;
}

int
setAllSessionsOffline(struct desk *desk)
{
	struct dbh *dbh = peekDB(desk->dbh.auth);
	if (dbh == NULL)
		return -1;

	dbhExecute(dbh, "\
UPDATE journal.sessions \
SET satellite_task_id = NULL \
WHERE satellite_task_id IS NOT NULL");

	if (!dbhCommandOK(dbh, dbh->result)) {
		pokeDB(dbh);
		return -1;
	}

	pokeDB(dbh);

	return 0;
}

int
setSessionOnline(struct task *task)
{
	struct dbh *dbh = peekDB(task->desk->dbh.auth);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 satelliteTaskId = htobe32(task->taskId);

    dbhPushBIGINT(dbh, &task->sessionId);
    dbhPushINTEGER(dbh, &satelliteTaskId);

	dbhExecute(dbh, "\
UPDATE journal.sessions \
SET satellite_task_id = $2 \
WHERE session_id = $1");

	if (!dbhCommandOK(dbh, dbh->result)) {
    	pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    dbhPushBIGINT(dbh, &task->deviceId);

	dbhExecute(dbh, "\
UPDATE journal.device_displacements \
SET need_on_radar_revision = TRUE, \
    need_in_sight_revision = TRUE \
WHERE device_id = $1");

	if (!dbhCommandOK(dbh, dbh->result)) {
    	pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	pokeDB(dbh);

	return 0;
}

int
setSessionOffline(struct task *task)
{
	struct dbh *dbh = peekDB(task->desk->dbh.auth);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    dbhPushBIGINT(dbh, &task->sessionId);

	dbhExecute(dbh, "\
UPDATE journal.sessions \
SET satellite_task_id = NULL \
WHERE session_id = $1");

	if (!dbhCommandOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    dbhPushBIGINT(dbh, &task->deviceId);

	dbhExecute(dbh, "\
UPDATE journal.device_displacements \
SET need_on_radar_revision = FALSE, \
    need_in_sight_revision = FALSE \
WHERE device_id = $1");

	if (!dbhCommandOK(dbh, dbh->result)) {
    	pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	pokeDB(dbh);

	return 0;
}

int
getSessionRevisions(
    struct task         *task,
    struct revisions    *revisions)
{
	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    dbhPushBIGINT(dbh, &task->sessionId);

	dbhExecute(dbh, "\
SELECT on_radar_revision, in_sight_revision, on_map_revision \
FROM journal.sessions \
WHERE session_id = $1");

	if (!dbhTuplesOK(dbh, dbh->result)) {
        pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 3)) {
        pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
        pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, INT4OID)) {
        pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 1, INT4OID)) {
        pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 2, INT4OID)) {
        pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    revisions->onRadar = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 0));
    revisions->inSight = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 1));
    revisions->onMap = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 2));

    pokeDB(dbh);

	return 0;
}

int
getSessionOnRadarRevision(
    struct task *task,
    struct dbh  *dbh,
    uint32      *revision)
{
    dbhPushBIGINT(dbh, &task->sessionId);

	dbhExecute(dbh, "\
SELECT in_sight_revision \
FROM journal.sessions \
WHERE session_id = $1");

	if (!dbhTuplesOK(dbh, dbh->result)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 1)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, INT4OID)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    *revision = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 0));

	return 0;
}

int
getSessionInSightRevision(
    struct task *task,
    struct dbh  *dbh,
    uint32      *revision)
{
    dbhPushBIGINT(dbh, &task->sessionId);

	dbhExecute(dbh, "\
SELECT in_sight_revision \
FROM journal.sessions \
WHERE session_id = $1");

	if (!dbhTuplesOK(dbh, dbh->result)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 1)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, INT4OID)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    *revision = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 0));

	return 0;
}

int
getSessionOnMapRevision(
    struct task *task,
    struct dbh  *dbh,
    uint32      *revision)
{
    dbhPushBIGINT(dbh, &task->sessionId);

	dbhExecute(dbh, "\
SELECT on_map_revision \
FROM journal.sessions \
WHERE session_id = $1");

	if (!dbhTuplesOK(dbh, dbh->result)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 1)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, INT4OID)) {
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    *revision = be32toh(*(uint32 *)PQgetvalue(dbh->result, 0, 0));

	return 0;
}
