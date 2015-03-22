#include <string.h>
#include "api.h"
#include "db.h"
#include "tasks.h"

uint64_t getSessionForDevice(struct task *task, struct dbh *dbh,
    uint64_t deviceId, uint64_t *sessionId,
    char *knownSessionToken, char *givenSessionToken)
{
	PGresult	*result;
	const char*	paramValues[2];
    Oid			paramTypes[2];
    int			paramLengths[2];
	int			paramFormats[2];

	paramValues   [0] = (char *)&deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(deviceId);
	paramFormats  [0] = 1;

	paramValues   [1] = knownSessionToken;
	paramTypes    [1] = UUIDOID;
	paramLengths  [1] = TokenBinarySize;
	paramFormats  [1] = 1;

	result = PQexecParams(dbh->conn, "SELECT session_id, session_token FROM journal.get_session($1, $2)",
		2, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 2)) {
		PQclear(result);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(result, 1)) {
		PQclear(result);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(result, 0, INT8OID) ||
	    !dbhCorrectColumnType(result, 1, UUIDOID)) {
		PQclear(result);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	memcpy(sessionId, PQgetvalue(result, 0, 0), sizeof(uint64_t));
	memcpy(givenSessionToken, PQgetvalue(result, 0, 1), TokenBinarySize);

	PQclear(result);

	return 0;
}

int getSessionNextInSightRevision(struct task *task, struct dbh *dbh, uint32_t *nextInSightRevision)
{
	PGresult	*result;
	const char*	paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	paramValues   [0] = (char *)&task->sessionId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->sessionId);
	paramFormats  [0] = 1;

	result = PQexecParams(dbh->conn, "UPDATE journal.sessions SET in_sight_revision = in_sight_revision + 1 WHERE session_id = $1 RETURNING in_sight_revision",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
		PQclear(result);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(result, 1)) {
		PQclear(result);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(result, 0, INT4OID)) {
		PQclear(result);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	memcpy(nextInSightRevision, PQgetvalue(result, 0, 0), sizeof(uint32_t));

	PQclear(result);

	return 0;
}
