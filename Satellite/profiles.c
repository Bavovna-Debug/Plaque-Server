#include <c.h>
#include <string.h>

#include "api.h"
#include "db.h"
#include "buffers.h"
#include "desk.h"
#include "paquet.h"
#include "report.h"
#include "tasks.h"

int
getProfiles(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	const char*	paramValues   [1];
    Oid			paramTypes    [1];
    int			paramLengths  [1];
	int			paramFormats  [1];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = NULL;

	uint32_t numberOfProfiles;

	if (paquet->payloadSize < sizeof(numberOfProfiles)) {
#ifdef DEBUG
		reportLog("Wrong payload size %d", paquet->payloadSize);
#endif
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	inputBuffer = getUInt32(inputBuffer, &numberOfProfiles);

	if (paquet->payloadSize != sizeof(numberOfProfiles) + numberOfProfiles * TokenBinarySize) {
#ifdef DEBUG
		reportLog("Wrong payload size %d for %d profiles", paquet->payloadSize, numberOfProfiles);
#endif
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	outputBuffer = peekBufferOfSize(task->desk->pools.dynamic, KB);
	if (outputBuffer == NULL) {
		setTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
		return -1;
	}

	paquet->outputBuffer = outputBuffer;

	resetBufferData(outputBuffer, 1);

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	int i;
	for (i = 0; i < numberOfProfiles; i++)
	{
		char profileToken[TokenBinarySize];

		inputBuffer = getData(inputBuffer, profileToken, TokenBinarySize);

		paramValues   [0] = (char *)&profileToken;
		paramTypes    [0] = UUIDOID;
		paramLengths  [0] = TokenBinarySize;
		paramFormats  [0] = 1;

        if (dbh->result != NULL)
        	PQclear(dbh->result);

		dbh->result = PQexecParams(dbh->conn, "\
SELECT profile_revision, profile_name, user_name \
FROM auth.profiles \
WHERE profile_token = $1",
			1, paramTypes, paramValues, paramLengths, paramFormats, 1);

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

		if (!dbhCorrectColumnType(dbh->result, 0, INT4OID) ||
    		!dbhCorrectColumnType(dbh->result, 1, VARCHAROID) ||
			!dbhCorrectColumnType(dbh->result, 2, VARCHAROID))
		{
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		char *profileRevision = PQgetvalue(dbh->result, 0, 0);
		char *profileName = PQgetvalue(dbh->result, 0, 1);
		int profileNameSize = PQgetlength(dbh->result, 0, 1);
		char *userName = PQgetvalue(dbh->result, 0, 2);
		int userNameSize = PQgetlength(dbh->result, 0, 2);

		if ((profileRevision == NULL) || (profileName == NULL) || (userName == NULL)) {
#ifdef DEBUG
			reportLog("No results");
#endif
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = putData(outputBuffer, profileToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, profileRevision, sizeof(uint32_t));
		outputBuffer = putString(outputBuffer, profileName, profileNameSize);
		outputBuffer = putString(outputBuffer, userName, userNameSize);
	}

	pokeDB(dbh);

	return 0;
}
