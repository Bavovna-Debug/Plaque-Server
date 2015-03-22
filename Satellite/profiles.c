#include <string.h>
#include "api.h"
#include "db.h"
#include "buffers.h"
#include "paquet.h"
#include "tasks.h"

#define DEBUG

int getProfiles(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	PGresult	*result;
	const char*	paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = NULL;

	uint32_t numberOfProfiles;

	if (paquet->payloadSize < sizeof(numberOfProfiles)) {
#ifdef DEBUG
		fprintf(stderr, "Wrong payload size %d\n", paquet->payloadSize);
#endif
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	inputBuffer = getUInt32(inputBuffer, &numberOfProfiles);

	if (paquet->payloadSize != sizeof(numberOfProfiles) + numberOfProfiles * TokenBinarySize) {
#ifdef DEBUG
		fprintf(stderr, "Wrong payload size %d for %d profiles\n", paquet->payloadSize, numberOfProfiles);
#endif
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	outputBuffer = peekBuffer(BUFFER1K);
	if (outputBuffer == NULL) {
		setTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
		return -1;
	}

	paquet->outputBuffer = outputBuffer;

	resetBufferData(outputBuffer, 1);

	struct dbh *dbh = peekDB(DB_PLAQUES_SESSION);
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

		result = PQexecParams(dbh->conn, "SELECT profile_revision, profile_name, user_name FROM auth.profiles WHERE profile_token = $1",
			1, paramTypes, paramValues, paramLengths, paramFormats, 1);

		if (!dbhTuplesOK(dbh, result)) {
			PQclear(result);
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		if (!dbhCorrectNumberOfColumns(result, 3)) {
			PQclear(result);
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		if (!dbhCorrectNumberOfRows(result, 1)) {
			PQclear(result);
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 0, INT4OID) ||
    		!dbhCorrectColumnType(result, 1, VARCHAROID) ||
			!dbhCorrectColumnType(result, 2, VARCHAROID))
		{
			PQclear(result);
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		char *profileRevision = PQgetvalue(result, 0, 0);
		char *profileName = PQgetvalue(result, 0, 1);
		int profileNameSize = PQgetlength(result, 0, 1);
		char *userName = PQgetvalue(result, 0, 2);
		int userNameSize = PQgetlength(result, 0, 2);

		if ((profileRevision == NULL) || (profileName == NULL) || (userName == NULL)) {
#ifdef DEBUG
			fprintf(stderr, "No results\n");
#endif
			PQclear(result);
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = putData(outputBuffer, profileToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, profileRevision, sizeof(uint32_t));
		outputBuffer = putString(outputBuffer, profileName, profileNameSize);
		outputBuffer = putString(outputBuffer, userName, userNameSize);

		PQclear(result);
	}

	pokeDB(dbh);

	return 0;
}
