#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "api.h"
#include "anticipant.h"
#include "db.h"
#include "tasks.h"

int verifyGuest(struct task *task)
{
	PGresult	*result;
	const char*	paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	struct dbh *dbh = peekDB(DB_CHAIN_GUARDIAN);
	if (dbh == NULL) {
		fprintf(stderr, "No database handler available\n");
		return -1;
	}

	printf("Verify IP: %s\n", task->clientIP);

	paramValues   [0] = task->clientIP;
	paramTypes    [0] = INETOID;
	paramLengths  [0] = strlen(task->clientIP);
	paramFormats  [0] = 0;

	result = PQexecParams(dbh->conn, "SELECT pool.verify_ip($1)",
		1, paramTypes, paramValues, paramLengths, paramFormats, 0);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		pokeDB(dbh);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
		PQclear(result);
		pokeDB(dbh);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(result, 1)) {
		PQclear(result);
		pokeDB(dbh);
		return -1;
	}

	int ipOK = (strcmp(PQgetvalue(result, 0, 0), "t") == 0) ? 0 : -1;

	PQclear(result);

	pokeDB(dbh);

	return ipOK;
}

int registerDevice(struct dbh *dbh, struct task *task)
{
	PGresult	*result;
	const char*	paramValues[5];
    Oid			paramTypes[5];
    int			paramLengths[5];
	int			paramFormats[5];

	if (verifyGuest(task) != 0)
		return -1;

	struct buffer *request = task->request;
	struct buffer *response = request;

	uint32_t payloadSize = bonjourGetPayloadSize(request);

	if (payloadSize != sizeof(struct bonjourAnticipant)) {
		fprintf(stderr, "Wrong payload size\n");
		return -1;
	}

	resetCursor(request, 1);

	struct bonjourAnticipant *anticipant = (struct bonjourAnticipant *)request->cursor;

	paramValues   [0] = (char *)&anticipant->vendorToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = TOKEN_SIZE;
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&anticipant->deviceName;
	paramTypes    [1] = VARCHAROID;
	paramLengths  [1] = strnlen(anticipant->deviceName, BonjourDeviceNameLength);
	paramFormats  [1] = 0;

	paramValues   [2] = (char *)&anticipant->deviceModel;
	paramTypes    [2] = VARCHAROID;
	paramLengths  [2] = strnlen(anticipant->deviceModel, BonjourDeviceModelLength);
	paramFormats  [2] = 0;

	paramValues   [3] = (char *)&anticipant->systemName;
	paramTypes    [3] = VARCHAROID;
	paramLengths  [3] = strnlen(anticipant->systemName, BonjourSystemNamelLength);
	paramFormats  [3] = 0;

	paramValues   [4] = (char *)&anticipant->systemVersion;
	paramTypes    [4] = VARCHAROID;
	paramLengths  [4] = strnlen(anticipant->systemVersion, BonjourSystemVersionlLength);
	paramFormats  [4] = 0;

	result = PQexecParams(dbh->conn, "SELECT auth.register_device($1, $2, $3, $4, $5)",
		5, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(result, 1)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		return -1;
	}

	resetBufferData(response, 1);

	uint32_t status = BonjourAnticipantOK;

	response = putUInt32(response, &status);

	char *deviceToken = PQgetvalue(result, 0, 0);

	response = putData(response, deviceToken, TOKEN_SIZE);

	PQclear(result);

	task->response = response;

	return 0;
}

int validateProfileName(struct dbh *dbh, struct task *task)
{
	PGresult	*result;
	const char*	paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	if (deviceIdByToken(dbh, bonjourDeviceToken(task->request)) == 0)
		return -1;

	struct buffer *request = task->request;
	struct buffer *response = request;

	uint32_t payloadSize = bonjourGetPayloadSize(request);

	if (payloadSize != sizeof(struct bonjourProfileNameValidation)) {
		fprintf(stderr, "Wrong payload size\n");
		return -1;
	}

	resetCursor(request, 1);

	struct bonjourProfileNameValidation *validation = (struct bonjourProfileNameValidation *)request->cursor;

	paramValues   [0] = (char *)&validation->profileName;
	paramTypes    [0] = VARCHAROID;
	paramLengths  [0] = strnlen(validation->profileName, BonjourProfileNameLength);
	paramFormats  [0] = 0;

	result = PQexecParams(dbh->conn, "SELECT COUNT(*) FROM auth.profiles WHERE profile_name = $1",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(result, 1)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectColumnType(result, 0, INT8OID)) {
		PQclear(result);
		return -1;
	}

	resetBufferData(response, 1);

	uint64_t count = dbhGetUInt64(result, 0, 0);

	uint32_t status = (count == 0) ? BonjourProfileNameAvailable : BonjourProfileNameAlreadyInUse;

	response = putUInt32(response, &status);

	PQclear(result);

	task->response = response;

	return 0;
}

int createProfile(struct dbh *dbh, struct task *task)
{
	PGresult	*result;
	const char*	paramValues[4];
    Oid			paramTypes[4];
    int			paramLengths[4];
	int			paramFormats[4];

	if (deviceIdByToken(dbh, bonjourDeviceToken(task->request)) == 0)
		return -1;

	struct buffer *request = task->request;
	struct buffer *response = request;

	uint32_t payloadSize = bonjourGetPayloadSize(request);

	if (payloadSize != sizeof(struct bonjourCreateProfile)) {
		fprintf(stderr, "Wrong payload size\n");
		return -1;
	}

	resetCursor(request, 1);

	struct bonjourCreateProfile *profile = (struct bonjourCreateProfile *)request->cursor;

	paramValues   [0] = (char *)&profile->profileName;
	paramTypes    [0] = VARCHAROID;
	paramLengths  [0] = strnlen(profile->profileName, BonjourProfileNameLength);
	paramFormats  [0] = 0;

	result = PQexecParams(dbh->conn, "INSERT INTO auth.profiles (profile_name) VALUES (TRIM($1)) RETURNING profile_id",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (sqlState(result, CHECK_VIOLATION)) {
		PQclear(result);

		resetBufferData(response, 1);

		uint32_t status = BonjourCreateProfileNameConstraint;

		response = putUInt32(response, &status);

		task->response = response;

		return 0;
	}

	if (sqlState(result, UNIQUE_VIOLATION)) {
		PQclear(result);

		resetBufferData(response, 1);

		uint32_t status = BonjourCreateProfileNameAlreadyInUse;

		response = putUInt32(response, &status);

		task->response = response;

		return 0;
	}

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(result, 1)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectColumnType(result, 0, INT8OID)) {
		PQclear(result);
		return -1;
	}

	uint64_t profileIdBigEndian;
	memcpy(&profileIdBigEndian, PQgetvalue(result, 0, 0), sizeof(profileIdBigEndian));

	PQclear(result);

	paramValues   [0] = (char *)&profileIdBigEndian;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(profileIdBigEndian);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&profile->userName;
	paramTypes    [1] = VARCHAROID;
	paramLengths  [1] = strnlen(profile->userName, BonjourUserNameLength);
	paramFormats  [1] = 0;

	if (strnlen(profile->passwordMD5, BonjourMD5Length) == 0) {
		paramValues   [2] = NULL;
		paramTypes    [2] = CHAROID;
		paramLengths  [2] = 0;
		paramFormats  [2] = 0;
	} else {
		paramValues   [2] = (char *)&profile->passwordMD5;
		paramTypes    [2] = CHAROID;
		paramLengths  [2] = strnlen(profile->passwordMD5, BonjourMD5Length);
		paramFormats  [2] = 0;
	}

	if (strnlen(profile->emailAddress, BonjourEmailAddressLength) == 0) {
		paramValues   [3] = NULL;
		paramTypes    [3] = VARCHAROID;
		paramLengths  [3] = 0;
		paramFormats  [3] = 0;
	} else {
		paramValues   [3] = (char *)&profile->emailAddress;
		paramTypes    [3] = VARCHAROID;
		paramLengths  [3] = strnlen(profile->emailAddress, BonjourEmailAddressLength);
		paramFormats  [3] = 0;
	}

	result = PQexecParams(dbh->conn, "UPDATE auth.profiles SET user_name = TRIM($2), password_md5 = $3, email_address = TRIM($4) WHERE profile_id = $1 RETURNING profile_token",
		4, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (sqlState(result, CHECK_VIOLATION)) {
		PQclear(result);

		resetBufferData(response, 1);

		uint32_t status = BonjourCreateProfileEmailConstraint;

		response = putUInt32(response, &status);

		task->response = response;

		return 0;
	}

	if (sqlState(result, UNIQUE_VIOLATION)) {
		PQclear(result);

		resetBufferData(response, 1);

		uint32_t status = BonjourCreateProfileEmailAlreadyInUse;

		response = putUInt32(response, &status);

		task->response = response;

		return 0;
	}

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(result, 1)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		return -1;
	}

	resetBufferData(response, 1);

	char *profileToken = PQgetvalue(result, 0, 0);
	if (profileToken == NULL) {
		fprintf(stderr, "No results\n");
		PQclear(result);
		return -1;
	}

	uint32_t status = BonjourCreateSucceeded;

	response = putUInt32(response, &status);

	response = putData(response, profileToken, TOKEN_SIZE);

	PQclear(result);

	task->response = response;

	return 0;
}
