#include <c.h>
#include <string.h>

#include "api.h"
#include "db.h"
#include "tasks.h"

uint64
getSessionForDevice(
    struct task *task,
    struct dbh *dbh,
    uint64 deviceId,
    uint64 *sessionId,
    char *knownSessionToken,
    char *givenSessionToken)
{
	const char*	paramValues   [2];
    Oid			paramTypes    [2];
    int			paramLengths  [2];
	int			paramFormats  [2];

	paramValues   [0] = (char *)&deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(deviceId);
	paramFormats  [0] = 1;

	paramValues   [1] = knownSessionToken;
	paramTypes    [1] = UUIDOID;
	paramLengths  [1] = TokenBinarySize;
	paramFormats  [1] = 1;

	dbh->result = PQexecParams(dbh->conn, "\
SELECT session_id, session_token \
FROM journal.get_session($1, $2)",
		2, paramTypes, paramValues, paramLengths, paramFormats, 1);

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
	const char	*paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	struct dbh *dbh = peekDB(desk->dbh.auth);
	if (dbh == NULL)
		return -1;

	dbh->result = PQexecParams(dbh->conn, "\
UPDATE journal.sessions \
SET satellite_task_id = NULL \
WHERE satellite_task_id IS NOT NULL",
		0, NULL, NULL, NULL, NULL, 1);

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
	const char	*paramValues  [2];
    Oid			paramTypes    [2];
    int			paramLengths  [2];
	int			paramFormats  [2];

	struct dbh *dbh = peekDB(task->desk->dbh.auth);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->sessionId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->sessionId);
	paramFormats  [0] = 1;

    uint32 satelliteTaskId = htobe32(task->taskId);

	paramValues   [1] = (char *)&satelliteTaskId;
	paramTypes    [1] = INT4OID;
	paramLengths  [1] = sizeof(satelliteTaskId);
	paramFormats  [1] = 1;

	dbh->result = PQexecParams(dbh->conn, "\
UPDATE journal.sessions \
SET satellite_task_id = $2 \
WHERE session_id = $1",
		2, paramTypes, paramValues, paramLengths, paramFormats, 1);

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
	const char	*paramValues  [1];
    Oid			paramTypes    [1];
    int			paramLengths  [1];
	int			paramFormats  [1];

	struct dbh *dbh = peekDB(task->desk->dbh.auth);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->sessionId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->sessionId);
	paramFormats  [0] = 1;

	dbh->result = PQexecParams(dbh->conn, "\
UPDATE journal.sessions \
SET satellite_task_id = NULL \
WHERE session_id = $1",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhCommandOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	pokeDB(dbh);

	return 0;
}

int
getSessionNextOnRadarRevision(
    struct task *task,
    struct dbh *dbh,
    uint32 *nextOnRadarRevision)
{
	const char*	paramValues   [1];
    Oid			paramTypes    [1];
    int			paramLengths  [1];
	int			paramFormats  [1];

	paramValues   [0] = (char *)&task->sessionId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->sessionId);
	paramFormats  [0] = 1;

	dbh->result = PQexecParams(dbh->conn, "\
UPDATE journal.sessions \
SET on_radar_revision = on_radar_revision + 1 \
WHERE session_id = $1 \
RETURNING on_radar_revision",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

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

	memcpy(nextOnRadarRevision, PQgetvalue(dbh->result, 0, 0), sizeof(uint32));

	return 0;
}

int
getSessionNextInSightRevision(
    struct task *task,
    struct dbh *dbh,
    uint32 *nextInSightRevision)
{
	const char*	paramValues   [1];
    Oid			paramTypes    [1];
    int			paramLengths  [1];
	int			paramFormats  [1];

	paramValues   [0] = (char *)&task->sessionId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->sessionId);
	paramFormats  [0] = 1;

	dbh->result = PQexecParams(dbh->conn, "\
UPDATE journal.sessions \
SET in_sight_revision = in_sight_revision + 1 \
WHERE session_id = $1 \
RETURNING in_sight_revision",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

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

	memcpy(nextInSightRevision, PQgetvalue(dbh->result, 0, 0), sizeof(uint32));

	return 0;
}
