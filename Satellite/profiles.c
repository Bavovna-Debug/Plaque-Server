#include <c.h>
#include <string.h>

#include "api.h"
#include "chalkboard.h"
#include "db.h"
#include "mmps.h"
#include "paquet.h"
#include "report.h"
#include "tasks.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

int
GetProfiles(struct Paquet *paquet)
{
#define QUERY_SELECT_PROFILES "\
SELECT profile_revision, profile_name, user_name \
FROM auth.profiles \
WHERE profile_token = $1"

	struct Task	*task = paquet->task;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = NULL;

	uint32 numberOfProfiles;

	if (paquet->payloadSize < sizeof(numberOfProfiles))
	{
#ifdef DEBUG
		ReportInfo("Wrong payload size %d", paquet->payloadSize);
#endif
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	inputBuffer = MMPS_GetInt32(inputBuffer, &numberOfProfiles);

	if (paquet->payloadSize != sizeof(numberOfProfiles) + numberOfProfiles * API_TokenBinarySize)
	{
#ifdef DEBUG
		ReportInfo("Wrong payload size %d for %d profiles", paquet->payloadSize, numberOfProfiles);
#endif
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	outputBuffer = MMPS_PeekBufferOfSize(chalkboard->pools.dynamic, KB, BUFFER_PROFILES);
	if (outputBuffer == NULL)
	{
		SetTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
		return -1;
	}

	paquet->outputBuffer = outputBuffer;

	MMPS_ResetBufferData(outputBuffer);

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL)
	{
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	int i;
	for (i = 0; i < numberOfProfiles; i++)
	{
		char profileToken[API_TokenBinarySize];

		inputBuffer = MMPS_GetData(inputBuffer, profileToken, API_TokenBinarySize, NULL);

        DB_PushUUID(dbh, (char *)&profileToken);

    	DB_Execute(dbh, QUERY_SELECT_PROFILES);

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

		if (!DB_CorrectColumnType(dbh->result, 0, INT4OID) ||
    		!DB_CorrectColumnType(dbh->result, 1, VARCHAROID) ||
			!DB_CorrectColumnType(dbh->result, 2, VARCHAROID))
		{
			DB_PokeHandle(dbh);
			SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		char *profileRevision = PQgetvalue(dbh->result, 0, 0);
		char *profileName = PQgetvalue(dbh->result, 0, 1);
		int profileNameSize = PQgetlength(dbh->result, 0, 1);
		char *userName = PQgetvalue(dbh->result, 0, 2);
		int userNameSize = PQgetlength(dbh->result, 0, 2);

		if ((profileRevision == NULL) || (profileName == NULL) || (userName == NULL))
		{
#ifdef DEBUG
			ReportInfo("No results");
#endif
			DB_PokeHandle(dbh);
			SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = MMPS_PutData(outputBuffer, profileToken, API_TokenBinarySize);
		outputBuffer = MMPS_PutData(outputBuffer, profileRevision, sizeof(uint32));
		outputBuffer = MMPS_PutString(outputBuffer, profileName, profileNameSize);
		outputBuffer = MMPS_PutString(outputBuffer, userName, userNameSize);
	}

	DB_PokeHandle(dbh);

	return 0;
}

uint64
ProfileIdByToken(struct dbh *dbh, char *profileToken)
{
#define QUERY_SEARCH_PROFILE_BY_TOKEN "\
SELECT profile_id \
FROM auth.profiles \
WHERE profile_token = $1"

    DB_PushUUID(dbh, profileToken);

	DB_Execute(dbh, QUERY_SEARCH_PROFILE_BY_TOKEN);

	if (!DB_TuplesOK(dbh, dbh->result))
		return 0;

	if (PQnfields(dbh->result) != 1)
		return 0;

	if (PQntuples(dbh->result) != 1)
		return 0;

	if (PQftype(dbh->result, 0) != INT8OID)
		return 0;

	uint64 profileIdBigEndian;
	memcpy(&profileIdBigEndian, PQgetvalue(dbh->result, 0, 0), sizeof(profileIdBigEndian));

	return profileIdBigEndian;
}
