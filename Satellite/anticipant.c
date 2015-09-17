#include <c.h>
#include <string.h>

#include "api.h"
#include "anticipant.h"
#include "buffers.h"
#include "db.h"
#include "report.h"
#include "tasks.h"

int
verifyGuest(struct task *task)
{
	const char	*paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	struct dbh *dbh = peekDB(task->desk->dbh.guardian);
	if (dbh == NULL) {
		reportError("No database handler available");
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->clientIP;
	paramTypes    [0] = INETOID;
	paramLengths  [0] = strlen(task->clientIP);
	paramFormats  [0] = 0;

	dbh->result = PQexecParams(dbh->conn,
		"SELECT pool.verify_ip($1)",
		1, paramTypes, paramValues, paramLengths, paramFormats, 0);

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	int ipOK = (strcmp(PQgetvalue(dbh->result, 0, 0), "t") == 0) ? 0 : -1;

	pokeDB(dbh);

	return ipOK;
}

int
registerDevice(
	struct task *task,
	struct dialogueAnticipant *anticipant,
	char *deviceToken)
{
	const char	*paramValues[5];
    Oid			paramTypes[5];
    int			paramLengths[5];
	int			paramFormats[5];

	if (verifyGuest(task) != 0)
		return -1;

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&anticipant->vendorToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = TokenBinarySize;
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&anticipant->deviceName;
	paramTypes    [1] = VARCHAROID;
	paramLengths  [1] = strnlen(anticipant->deviceName, AnticipantDeviceNameLength);
	paramFormats  [1] = 0;

	paramValues   [2] = (char *)&anticipant->deviceModel;
	paramTypes    [2] = VARCHAROID;
	paramLengths  [2] = strnlen(anticipant->deviceModel, AnticipantDeviceModelLength);
	paramFormats  [2] = 0;

	paramValues   [3] = (char *)&anticipant->systemName;
	paramTypes    [3] = VARCHAROID;
	paramLengths  [3] = strnlen(anticipant->systemName, AnticipantSystemNamelLength);
	paramFormats  [3] = 0;

	paramValues   [4] = (char *)&anticipant->systemVersion;
	paramTypes    [4] = VARCHAROID;
	paramLengths  [4] = strnlen(anticipant->systemVersion, AnticipantSystemVersionlLength);
	paramFormats  [4] = 0;

	dbh->result = PQexecParams(dbh->conn,
		"SELECT auth.register_device($1, $2, $3, $4, $5)",
		5, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, UUIDOID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	char *queryResult = PQgetvalue(dbh->result, 0, 0);

	memcpy(deviceToken, queryResult, TokenBinarySize);

	pokeDB(dbh);

	return 0;
}

int
validateProfileName(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	const char	*paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	//if (deviceIdByToken(dbh, bonjourDeviceToken(task->request)) == 0)
		//return -1;

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct bonjourProfileNameValidation))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct bonjourProfileNameValidation validation;

	inputBuffer = getData(inputBuffer, (char *)&validation, sizeof(validation));

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&validation.profileName;
	paramTypes    [0] = VARCHAROID;
	paramLengths  [0] = strnlen(validation.profileName, BonjourProfileNameLength);
	paramFormats  [0] = 0;

	dbh->result = PQexecParams(dbh->conn,
		"SELECT auth.is_profile_name_free($1)",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, BOOLOID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	uint32 status = (isPostgresBooleanTrue(PQgetvalue(dbh->result, 0, 0)))
		? PaquetProfileNameAvailable
		: PaquetProfileNameAlreadyInUse;

	outputBuffer = putUInt32(outputBuffer, &status);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
createProfile(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	const char	*paramValues[4];
    Oid			paramTypes[4];
    int			paramLengths[4];
	int			paramFormats[4];

	//if (deviceIdByToken(dbh, bonjourDeviceToken(task->request)) == 0)
		//return -1;

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct bonjourCreateProfile))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct bonjourCreateProfile profile;

	inputBuffer = getData(inputBuffer, (char *)&profile, sizeof(profile));

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&profile.profileName;
	paramTypes    [0] = VARCHAROID;
	paramLengths  [0] = strnlen(profile.profileName, BonjourProfileNameLength);
	paramFormats  [0] = 0;

	dbh->result = PQexecParams(dbh->conn,
		"INSERT INTO auth.profiles (profile_name) VALUES (TRIM($1)) RETURNING profile_id",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (sqlState(dbh->result, CHECK_VIOLATION)) {
		pokeDB(dbh);

		resetBufferData(outputBuffer, 1);

		uint32 status = BonjourCreateProfileNameConstraint;
		outputBuffer = putUInt32(outputBuffer, &status);

		paquet->outputBuffer = paquet->inputBuffer;

		return 0;
	}

	if (sqlState(dbh->result, UNIQUE_VIOLATION)) {
		pokeDB(dbh);

		resetBufferData(outputBuffer, 1);

		uint32 status = BonjourCreateProfileNameAlreadyInUse;
		outputBuffer = putUInt32(outputBuffer, &status);

		paquet->outputBuffer = paquet->inputBuffer;

		return 0;
	}

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, INT8OID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	uint64 profileIdBigEndian;
	memcpy(&profileIdBigEndian, PQgetvalue(dbh->result, 0, 0), sizeof(profileIdBigEndian));

	PQclear(dbh->result);

	paramValues   [0] = (char *)&profileIdBigEndian;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(profileIdBigEndian);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&profile.userName;
	paramTypes    [1] = VARCHAROID;
	paramLengths  [1] = strnlen(profile.userName, BonjourUserNameLength);
	paramFormats  [1] = 0;

	if (strnlen(profile.passwordMD5, BonjourMD5Length) == 0) {
		paramValues   [2] = NULL;
		paramTypes    [2] = CHAROID;
		paramLengths  [2] = 0;
		paramFormats  [2] = 0;
	} else {
		paramValues   [2] = (char *)&profile.passwordMD5;
		paramTypes    [2] = CHAROID;
		paramLengths  [2] = strnlen(profile.passwordMD5, BonjourMD5Length);
		paramFormats  [2] = 0;
	}

	if (strnlen(profile.emailAddress, BonjourEmailAddressLength) == 0) {
		paramValues   [3] = NULL;
		paramTypes    [3] = VARCHAROID;
		paramLengths  [3] = 0;
		paramFormats  [3] = 0;
	} else {
		paramValues   [3] = (char *)&profile.emailAddress;
		paramTypes    [3] = VARCHAROID;
		paramLengths  [3] = strnlen(profile.emailAddress, BonjourEmailAddressLength);
		paramFormats  [3] = 0;
	}

	dbh->result = PQexecParams(dbh->conn, "\
UPDATE auth.profiles \
SET user_name = TRIM($2), \
	password_md5 = $3, \
	email_address = TRIM($4) \
WHERE profile_id = $1 \
RETURNING profile_token",
		4, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (sqlState(dbh->result, CHECK_VIOLATION)) {
		pokeDB(dbh);

		resetBufferData(outputBuffer, 1);

		uint32 status = BonjourCreateProfileEmailConstraint;
		outputBuffer = putUInt32(outputBuffer, &status);

		paquet->outputBuffer = paquet->inputBuffer;

		return 0;
	}

	if (sqlState(dbh->result, UNIQUE_VIOLATION)) {
		pokeDB(dbh);

		resetBufferData(outputBuffer, 1);

		uint32 status = BonjourCreateProfileEmailAlreadyInUse;
		outputBuffer = putUInt32(outputBuffer, &status);

		paquet->outputBuffer = paquet->inputBuffer;

		return 0;
	}

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, UUIDOID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	char *profileToken = PQgetvalue(dbh->result, 0, 0);
	if (profileToken == NULL) {
		reportError("No results");
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	uint32 status = BonjourCreateSucceeded;

	outputBuffer = putUInt32(outputBuffer, &status);

	outputBuffer = putData(outputBuffer, profileToken, TokenBinarySize);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
notificationsToken(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	const char	*paramValues[2];
    Oid			paramTypes[2];
    int			paramLengths[2];
	int			paramFormats[2];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetNotificationsToken))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetNotificationsToken payload;

	inputBuffer = getData(inputBuffer, (char *)&payload, sizeof(payload));

/*
	char notificationsToken[NotificationsTokenStringSize];
	int byteOffset;
	for (byteOffset = 0; byteOffset < NotificationsTokenBinarySize; byteOffset++)
	{
		char *destination = (char *)&notificationsToken + byteOffset * 2;
		unsigned char source = (unsigned char)payload.notificationsToken[byteOffset];
		sprintf(destination, "%02x", source);
	}
*/

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->deviceId);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)payload.notificationsToken;
	paramTypes    [1] = BYTEAOID;
	paramLengths  [1] = NotificationsTokenBinarySize;
	paramFormats  [1] = 1;

	dbh->result = PQexecParams(dbh->conn,
		"SELECT journal.set_apns_token($1, $2)",
		2, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(dbh->result, 1)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, BOOLOID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	uint32 status;

	char answer = *PQgetvalue(dbh->result, 0, 0);
	if (answer == 1) {
		status = PaquetNotificationsTokenAccepted;
	} else {
		status = PaquetNotificationsTokenDeclined;
	}

	outputBuffer = putUInt32(outputBuffer, &status);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}
