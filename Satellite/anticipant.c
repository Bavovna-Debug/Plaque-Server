#include <c.h>
#include <string.h>

#include "api.h"
#include "anticipant.h"
#include "chalkboard.h"
#include "db.h"
#include "mmps.h"
#include "report.h"
#include "tasks.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

/**
 * VerifyGuest()
 *
 * @task:
 */
int
VerifyGuest(struct task *task)
{
#define QUERY_VERIFY_GUEST "\
SELECT pool.verify_ip($1)"

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.guardian);
	if (dbh == NULL)
	{
		reportError("No database handler available");
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	DB_PushArgument(dbh, (char *) &task->clientIP, INETOID, strlen(task->clientIP), 0);

	DB_Execute(dbh, QUERY_VERIFY_GUEST);

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	int ipOK = (*PQgetvalue(dbh->result, 0, 0) == 1) ? 0 : -1;

	DB_PokeHandle(dbh);

	return ipOK;
}

/**
 * RegisterDevice()
 *
 * @task:
 * @anticipant:
 * @deviceToken:
 */
int
RegisterDevice(
	struct task 				*task,
	struct DialogueAnticipant 	*anticipant,
	char 						*deviceToken)
{
#define QUERY_REGISTER_DEVICE "\
SELECT auth.register_device($1, $2, $3, $4, $5)"

	if (VerifyGuest(task) != 0)
		return -1;

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL)
	{
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	DB_PushArgument(dbh, (char *) &anticipant->vendorToken, UUIDOID, TokenBinarySize, 1);

	DB_PushVARCHAR(dbh,
		(char *) &anticipant->deviceName,
		strnlen(anticipant->deviceName, AnticipantDeviceNameLength));

	DB_PushVARCHAR(dbh,
		(char *) &anticipant->deviceModel,
		strnlen(anticipant->deviceModel, AnticipantDeviceModelLength));

	DB_PushVARCHAR(dbh,
		(char *) &anticipant->systemName,
		strnlen(anticipant->systemName, AnticipantSystemNamelLength));

	DB_PushVARCHAR(dbh,
		(char *) &anticipant->systemVersion,
		strnlen(anticipant->systemVersion, AnticipantSystemVersionlLength));

	DB_Execute(dbh, QUERY_REGISTER_DEVICE);

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	char *queryResult = PQgetvalue(dbh->result, 0, 0);

	memcpy(deviceToken, queryResult, TokenBinarySize);

	DB_PokeHandle(dbh);

	return 0;
}

/**
 * ValidateProfileName()
 *
 * @paquet:
 */
int
ValidateProfileName(struct paquet *paquet)
{
#define QUERY_VALIDATE_PROFILE_NAME "\
SELECT auth.is_profile_name_free($1)"

	struct task	*task = paquet->task;

	//if (deviceIdByToken(dbh, bonjourDeviceToken(task->request)) == 0)
		//return -1;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct BonjourProfileNameValidation)))
	{
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer, 1);

	struct BonjourProfileNameValidation validation;

	inputBuffer = MMPS_GetData(inputBuffer, (char *) &validation, sizeof(validation));

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL)
	{
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	DB_PushVARCHAR(dbh,
		(char *) &validation.profileName,
		strnlen(validation.profileName, BonjourProfileNameLength));

	DB_Execute(dbh, QUERY_VALIDATE_PROFILE_NAME);

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, BOOLOID)) {
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

	uint32 status = (*PQgetvalue(dbh->result, 0, 0) == 1)
		? PaquetProfileNameAvailable
		: PaquetProfileNameAlreadyInUse;

	outputBuffer = MMPS_PutInt32(outputBuffer, &status);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

/**
 * CreateProfile()
 *
 * @paquet:
 */
int
CreateProfile(struct paquet *paquet)
{
#define QUERY_CREATE_PROFILE "\
INSERT INTO auth.profiles (profile_name) \
VALUES (TRIM($1)) RETURNING profile_id"

#define QUERY_UPDATE_PROFILE "\
UPDATE auth.profiles \
SET user_name = TRIM($2), \
	password_md5 = $3, \
	email_address = TRIM($4) \
WHERE profile_id = $1 \
RETURNING profile_token"

	struct task	*task = paquet->task;

	//if (deviceIdByToken(dbh, bonjourDeviceToken(task->request)) == 0)
		//return -1;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct BonjourCreateProfile)))
	{
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer, 1);

	struct BonjourCreateProfile *profile =
		(struct BonjourCreateProfile *) inputBuffer->cursor;

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL)
	{
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    DB_PushVARCHAR(dbh,
    	(char *) &profile->profileName,
    	strnlen(profile->profileName, BonjourProfileNameLength));

	DB_Execute(dbh, QUERY_CREATE_PROFILE);

	if (DB_HasState(dbh->result, CHECK_VIOLATION))
	{
		DB_PokeHandle(dbh);

		MMPS_ResetBufferData(outputBuffer, 1);

		uint32 status = BonjourCreateProfileNameConstraint;
		outputBuffer = MMPS_PutInt32(outputBuffer, &status);

		paquet->outputBuffer = paquet->inputBuffer;

		return 0;
	}

	if (DB_HasState(dbh->result, UNIQUE_VIOLATION))
	{
		DB_PokeHandle(dbh);

		MMPS_ResetBufferData(outputBuffer, 1);

		uint32 status = BonjourCreateProfileNameAlreadyInUse;
		outputBuffer = MMPS_PutInt32(outputBuffer, &status);

		paquet->outputBuffer = paquet->inputBuffer;

		return 0;
	}

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, INT8OID))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	uint64 profileIdBigEndian;
	//memcpy(&profileIdBigEndian, PQgetvalue(dbh->result, 0, 0), sizeof(profileIdBigEndian));
    profileIdBigEndian = *(uint64 *) PQgetvalue(dbh->result, 0, 0);

    DB_PushBIGINT(dbh, &profileIdBigEndian);
    DB_PushVARCHAR(dbh,
    	(char *) &profile->userName,
    	strnlen(profile->userName, BonjourUserNameLength));

	if (strnlen(profile->passwordMD5, BonjourMD5Length) == 0) {
	    DB_PushCHAR(dbh, NULL, 0);
	} else {
	    DB_PushCHAR(dbh,
	    	(char *) &profile->passwordMD5,
	    	strnlen(profile->passwordMD5, BonjourMD5Length));
	}

	if (strnlen(profile->emailAddress, BonjourEmailAddressLength) == 0) {
	    DB_PushVARCHAR(dbh, NULL, 0);
	} else {
	    DB_PushVARCHAR(dbh,
	    	(char *) &profile->emailAddress,
	    	strnlen(profile->emailAddress, BonjourEmailAddressLength));
	}

	DB_Execute(dbh, QUERY_UPDATE_PROFILE);

	if (DB_HasState(dbh->result, CHECK_VIOLATION))
	{
		DB_PokeHandle(dbh);

		MMPS_ResetBufferData(outputBuffer, 1);

		uint32 status = BonjourCreateProfileEmailConstraint;
		outputBuffer = MMPS_PutInt32(outputBuffer, &status);

		paquet->outputBuffer = paquet->inputBuffer;

		return 0;
	}

	if (DB_HasState(dbh->result, UNIQUE_VIOLATION))
	{
		DB_PokeHandle(dbh);

		MMPS_ResetBufferData(outputBuffer, 1);

		uint32 status = BonjourCreateProfileEmailAlreadyInUse;
		outputBuffer = MMPS_PutInt32(outputBuffer, &status);

		paquet->outputBuffer = paquet->inputBuffer;

		return 0;
	}

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	char *profileToken = PQgetvalue(dbh->result, 0, 0);
	if (profileToken == NULL) {
		reportError("No results");
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

	uint32 status = BonjourCreateSucceeded;

	outputBuffer = MMPS_PutInt32(outputBuffer, &status);

	outputBuffer = MMPS_PutData(outputBuffer, profileToken, TokenBinarySize);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

/**
 * NotificationsToken()
 *
 * @paquet:
 */
int
NotificationsToken(struct paquet *paquet)
{
#define QUERY_SET_APNS_TOKEN "\
SELECT journal.set_apns_token($1, $2)"

	struct task	*task = paquet->task;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetNotificationsToken)))
	{
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer, 1);

	struct paquetNotificationsToken payload;

	inputBuffer = MMPS_GetData(inputBuffer, (char *) &payload, sizeof(payload));

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

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL)
	{
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    DB_PushBIGINT(dbh, &task->deviceId);
    DB_PushBYTEA(dbh,
    	(char *) &payload.notificationsToken,
    	NotificationsTokenBinarySize);

	DB_Execute(dbh, QUERY_SET_APNS_TOKEN);

	if (!DB_TuplesOK(dbh, dbh->result))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, BOOLOID))
	{
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

	uint32 status;

	char answer = *PQgetvalue(dbh->result, 0, 0);
	if (answer == 1) {
		status = PaquetNotificationsTokenAccepted;
	} else {
		status = PaquetNotificationsTokenDeclined;
	}

	outputBuffer = MMPS_PutInt32(outputBuffer, &status);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}
