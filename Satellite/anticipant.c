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
	struct dbh *dbh = peekDB(task->desk->dbh.guardian);
	if (dbh == NULL) {
		reportError("No database handler available");
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	dbhPushArgument(dbh, (char *)&task->clientIP, INETOID, strlen(task->clientIP), 0);

	dbhExecute(dbh, "SELECT pool.verify_ip($1)");

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

	int ipOK = (*PQgetvalue(dbh->result, 0, 0) == 1) ? 0 : -1;

	pokeDB(dbh);

	return ipOK;
}

int
registerDevice(
	struct task *task,
	struct dialogueAnticipant *anticipant,
	char *deviceToken)
{
	if (verifyGuest(task) != 0)
		return -1;

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	dbhPushArgument(dbh, (char *)&anticipant->vendorToken, UUIDOID, TokenBinarySize, 1);

	dbhPushVARCHAR(dbh,
		(char *)&anticipant->deviceName,
		strnlen(anticipant->deviceName, AnticipantDeviceNameLength));

	dbhPushVARCHAR(dbh,
		(char *)&anticipant->deviceModel,
		strnlen(anticipant->deviceModel, AnticipantDeviceModelLength));

	dbhPushVARCHAR(dbh,
		(char *)&anticipant->systemName,
		strnlen(anticipant->systemName, AnticipantSystemNamelLength));

	dbhPushVARCHAR(dbh,
		(char *)&anticipant->systemVersion,
		strnlen(anticipant->systemVersion, AnticipantSystemVersionlLength));

	dbhExecute(dbh, "SELECT auth.register_device($1, $2, $3, $4, $5)");

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

	dbhPushVARCHAR(dbh,
		(char *)&validation.profileName,
		strnlen(validation.profileName, BonjourProfileNameLength));

	dbhExecute(dbh, "SELECT auth.is_profile_name_free($1)");

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

	uint32 status = (*PQgetvalue(dbh->result, 0, 0) == 1)
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

	//if (deviceIdByToken(dbh, bonjourDeviceToken(task->request)) == 0)
		//return -1;

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct bonjourCreateProfile))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct bonjourCreateProfile *profile = (struct bonjourCreateProfile *)inputBuffer->cursor;

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    dbhPushVARCHAR(dbh,
    	(char *)&profile->profileName,
    	strnlen(profile->profileName, BonjourProfileNameLength));

	dbhExecute(dbh, "\
INSERT INTO auth.profiles (profile_name) VALUES (TRIM($1)) RETURNING profile_id");

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
	//memcpy(&profileIdBigEndian, PQgetvalue(dbh->result, 0, 0), sizeof(profileIdBigEndian));
    profileIdBigEndian = *(uint64 *)PQgetvalue(dbh->result, 0, 0);

    dbhPushBIGINT(dbh, &profileIdBigEndian);
    dbhPushVARCHAR(dbh,
    	(char *)&profile->userName,
    	strnlen(profile->userName, BonjourUserNameLength));

	if (strnlen(profile->passwordMD5, BonjourMD5Length) == 0) {
	    dbhPushCHAR(dbh, NULL, 0);
	} else {
	    dbhPushCHAR(dbh,
	    	(char *)&profile->passwordMD5,
	    	strnlen(profile->passwordMD5, BonjourMD5Length));
	}

	if (strnlen(profile->emailAddress, BonjourEmailAddressLength) == 0) {
	    dbhPushVARCHAR(dbh, NULL, 0);
	} else {
	    dbhPushVARCHAR(dbh,
	    	(char *)&profile->emailAddress,
	    	strnlen(profile->emailAddress, BonjourEmailAddressLength));
	}

	dbhExecute(dbh, "\
UPDATE auth.profiles \
SET user_name = TRIM($2), \
	password_md5 = $3, \
	email_address = TRIM($4) \
WHERE profile_id = $1 \
RETURNING profile_token");

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

    dbhPushBIGINT(dbh, &task->deviceId);
    dbhPushBYTEA(dbh,
    	(char *)&payload.notificationsToken,
    	NotificationsTokenBinarySize);

	dbhExecute(dbh, "\
SELECT journal.set_apns_token($1, $2)");

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
