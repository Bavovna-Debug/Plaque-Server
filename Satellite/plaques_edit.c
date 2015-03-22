#include <stdlib.h>
#include <string.h>
#include "api.h"
#include "db.h"
#include "buffers.h"
#include "paquet.h"
#include "plaques_edit.h"
#include "tasks.h"

#define DEBUG

int paquetPostNewPlaque(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	PGresult	*result;
	const char*	paramValues[13];
    Oid			paramTypes[13];
    int			paramLengths[13];
	int			paramFormats[13];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!minimumPayloadSize(paquet, sizeof(struct paquetPostPlaque))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetPostPlaque plaque;

	inputBuffer = getData(inputBuffer, (char *)&plaque, sizeof(plaque));

	int expectedSize = sizeof(plaque) + be32toh(plaque.inscriptionLength);
	if (!expectedPayloadSize(paquet, expectedSize)) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	char *inscription = (char *)malloc(plaque.inscriptionLength);
	if (inscription == NULL) {
#ifdef DEBUG
		fprintf(stderr, "Cannot allocate %ud bytes for inscription\n", plaque.inscriptionLength);
#endif
		setTaskStatus(task, TaskStatusOutOfMemory);
		return -1;
	}

	inputBuffer = getData(inputBuffer, inscription, plaque.inscriptionLength);

	struct dbh *dbh = peekDB(DB_PLAQUES_SESSION);
	if (dbh == NULL) {
		free(inscription);
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->deviceId);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&task->profileId;
	paramTypes    [1] = INT8OID;
	paramLengths  [1] = sizeof(task->profileId);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&plaque.latitude;
	paramTypes    [2] = FLOAT8OID;
	paramLengths  [2] = sizeof(plaque.latitude);
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)&plaque.longitude;
	paramTypes    [3] = FLOAT8OID;
	paramLengths  [3] = sizeof(plaque.longitude);
	paramFormats  [3] = 1;

	paramValues   [4] = (char *)&plaque.altitude;
	paramTypes    [4] = FLOAT4OID;
	paramLengths  [4] = sizeof(plaque.altitude);
	paramFormats  [4] = 1;

	if (plaque.directed == DeviceBooleanFalse) {
		paramValues   [5] = NULL;
		paramTypes    [5] = FLOAT4OID;
		paramLengths  [5] = 0;
		paramFormats  [5] = 1;
	} else {
		paramValues   [5] = (char *)&plaque.direction;
		paramTypes    [5] = FLOAT4OID;
		paramLengths  [5] = sizeof(plaque.direction);
		paramFormats  [5] = 1;
	}

	if (plaque.tilted == DeviceBooleanFalse) {
		paramValues   [6] = NULL;
		paramTypes    [6] = FLOAT4OID;
		paramLengths  [6] = 0;
		paramFormats  [6] = 1;
	} else {
		paramValues   [6] = (char *)&plaque.tilt;
		paramTypes    [6] = FLOAT4OID;
		paramLengths  [6] = sizeof(plaque.tilt);
		paramFormats  [6] = 1;
	}

	paramValues   [7] = (char *)&plaque.width;
	paramTypes    [7] = FLOAT4OID;
	paramLengths  [7] = sizeof(float);
	paramFormats  [7] = 1;

	paramValues   [8] = (char *)&plaque.height;
	paramTypes    [8] = FLOAT4OID;
	paramLengths  [8] = sizeof(plaque.height);
	paramFormats  [8] = 1;

	paramValues   [9] = (char *)&plaque.backgroundColor;
	paramTypes    [9] = INT4OID;
	paramLengths  [9] = sizeof(plaque.backgroundColor);
	paramFormats  [9] = 1;

	paramValues  [10] = (char *)&plaque.foregroundColor;
	paramTypes   [10] = INT4OID;
	paramLengths [10] = sizeof(plaque.foregroundColor);
	paramFormats [10] = 1;

	paramValues  [11] = (char *)&plaque.fontSize;
	paramTypes   [11] = FLOAT4OID;
	paramLengths [11] = sizeof(plaque.fontSize);
	paramFormats [11] = 1;

	paramValues  [12] = (char *)inscription;
	paramTypes   [12] = TEXTOID;
	paramLengths [12] = plaque.inscriptionLength;
	paramFormats [12] = 0;

	result = PQexecParams(dbh->conn, "INSERT INTO surrounding.plaques (device_id, profile_id, dimension, latitude, longitude, altitude, direction, tilt, width, height, background_color, foreground_color, font_size, inscription) VALUES ($1, $2, '3D', $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13) RETURNING plaque_token",
		13, paramTypes, paramValues, paramLengths, paramFormats, 1);

	free(inscription);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
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

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	uint32_t bonjourStatus = PaquetCreatePlaqueSucceeded;

	outputBuffer = putUInt32(outputBuffer, &bonjourStatus);

	char *plaqueToken = PQgetvalue(result, 0, 0);

	outputBuffer = putData(outputBuffer, plaqueToken, TokenBinarySize);

	PQclear(result);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int paquetChangePlaqueLocation(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	PGresult	*result;
	const char*	paramValues[6];
    Oid			paramTypes[6];
    int			paramLengths[6];
	int			paramFormats[6];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetPlaqueLocation))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetPlaqueLocation payload;

	inputBuffer = getData(inputBuffer, (char *)&payload, sizeof(payload));

	struct dbh *dbh = peekDB(DB_PLAQUES_SESSION);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(uint64_t);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&task->profileId;
	paramTypes    [1] = INT8OID;
	paramLengths  [1] = sizeof(uint64_t);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&payload.plaqueToken;
	paramTypes    [2] = UUIDOID;
	paramLengths  [2] = TokenBinarySize;
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)&payload.latitude;
	paramTypes    [3] = FLOAT8OID;
	paramLengths  [3] = sizeof(double);
	paramFormats  [3] = 1;

	paramValues   [4] = (char *)&payload.longitude;
	paramTypes    [4] = FLOAT8OID;
	paramLengths  [4] = sizeof(double);
	paramFormats  [4] = 1;

	paramValues   [5] = (char *)&payload.altitude;
	paramTypes    [5] = FLOAT4OID;
	paramLengths  [5] = sizeof(float);
	paramFormats  [5] = 1;

	result = PQexecParams(dbh->conn, "UPDATE surrounding.plaques SET latitude = $4, longitude = $5, altitude = $6 WHERE plaque_token = $3 RETURNING plaque_token",
		6, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
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

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	PQclear(result);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int paquetChangePlaqueOrientation(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	PGresult	*result;
	const char*	paramValues[5];
    Oid			paramTypes[5];
    int			paramLengths[5];
	int			paramFormats[5];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetPlaqueOrientation))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetPlaqueOrientation payload;

	inputBuffer = getData(inputBuffer, (char *)&payload, sizeof(payload));

	struct dbh *dbh = peekDB(DB_PLAQUES_SESSION);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(uint64_t);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&task->profileId;
	paramTypes    [1] = INT8OID;
	paramLengths  [1] = sizeof(uint64_t);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&payload.plaqueToken;
	paramTypes    [2] = UUIDOID;
	paramLengths  [2] = TokenBinarySize;
	paramFormats  [2] = 1;

	if (payload.directed == DeviceBooleanFalse) {
		paramValues   [3] = NULL;
		paramTypes    [3] = FLOAT4OID;
		paramLengths  [3] = 0;
		paramFormats  [3] = 1;
	} else {
		paramValues   [3] = (char *)&payload.direction;
		paramTypes    [3] = FLOAT4OID;
		paramLengths  [3] = sizeof(payload.direction);
		paramFormats  [3] = 1;
	}

	if (payload.tilted == DeviceBooleanFalse) {
		paramValues   [4] = NULL;
		paramTypes    [4] = FLOAT4OID;
		paramLengths  [4] = 0;
		paramFormats  [4] = 1;
	} else {
		paramValues   [4] = (char *)&payload.tilt;
		paramTypes    [4] = FLOAT4OID;
		paramLengths  [4] = sizeof(payload.tilt);
		paramFormats  [4] = 1;
	}

	result = PQexecParams(dbh->conn, "UPDATE surrounding.plaques SET direction = $4, tilt = $5 WHERE plaque_token = $3 RETURNING plaque_token",
		5, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
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

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	PQclear(result);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int paquetChangePlaqueSize(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	PGresult	*result;
	const char*	paramValues[5];
    Oid			paramTypes[5];
    int			paramLengths[5];
	int			paramFormats[5];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetPlaqueSize))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetPlaqueSize payload;

	inputBuffer = getData(inputBuffer, (char *)&payload, sizeof(payload));

	struct dbh *dbh = peekDB(DB_PLAQUES_SESSION);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(uint64_t);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&task->profileId;
	paramTypes    [1] = INT8OID;
	paramLengths  [1] = sizeof(uint64_t);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&payload.plaqueToken;
	paramTypes    [2] = UUIDOID;
	paramLengths  [2] = TokenBinarySize;
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)&payload.width;
	paramTypes    [3] = FLOAT4OID;
	paramLengths  [3] = sizeof(payload.width);
	paramFormats  [3] = 1;

	paramValues   [4] = (char *)&payload.height;
	paramTypes    [4] = FLOAT4OID;
	paramLengths  [4] = sizeof(payload.height);
	paramFormats  [4] = 1;

	result = PQexecParams(dbh->conn, "UPDATE surrounding.plaques SET width = $4, height = $5 WHERE plaque_token = $3 RETURNING plaque_token",
		5, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
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

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	PQclear(result);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int paquetChangePlaqueColors(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	PGresult	*result;
	const char*	paramValues[5];
    Oid			paramTypes[5];
    int			paramLengths[5];
	int			paramFormats[5];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetPlaqueColors))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetPlaqueColors payload;

	inputBuffer = getData(inputBuffer, (char *)&payload, sizeof(payload));

	struct dbh *dbh = peekDB(DB_PLAQUES_SESSION);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(uint64_t);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&task->profileId;
	paramTypes    [1] = INT8OID;
	paramLengths  [1] = sizeof(uint64_t);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&payload.plaqueToken;
	paramTypes    [2] = UUIDOID;
	paramLengths  [2] = TokenBinarySize;
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)&payload.backgroundColor;
	paramTypes    [3] = INT4OID;
	paramLengths  [3] = sizeof(payload.backgroundColor);
	paramFormats  [3] = 1;

	paramValues   [4] = (char *)&payload.foregroundColor;
	paramTypes    [4] = INT4OID;
	paramLengths  [4] = sizeof(payload.foregroundColor);
	paramFormats  [4] = 1;

	result = PQexecParams(dbh->conn, "UPDATE surrounding.plaques SET background_color = $4, foreground_color = $5 WHERE plaque_token = $3 RETURNING plaque_token",
		5, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
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

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	PQclear(result);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int paquetChangePlaqueFont(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	PGresult	*result;
	const char*	paramValues[4];
    Oid			paramTypes[4];
    int			paramLengths[4];
	int			paramFormats[4];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetPlaqueFont))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetPlaqueFont payload;

	inputBuffer = getData(inputBuffer, (char *)&payload, sizeof(payload));

	struct dbh *dbh = peekDB(DB_PLAQUES_SESSION);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(uint64_t);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&task->profileId;
	paramTypes    [1] = INT8OID;
	paramLengths  [1] = sizeof(uint64_t);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&payload.plaqueToken;
	paramTypes    [2] = UUIDOID;
	paramLengths  [2] = TokenBinarySize;
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)&payload.fontSize;
	paramTypes    [3] = FLOAT4OID;
	paramLengths  [3] = sizeof(payload.fontSize);
	paramFormats  [3] = 1;

	result = PQexecParams(dbh->conn, "UPDATE surrounding.plaques SET font_size = $4 WHERE plaque_token = $3 RETURNING plaque_token",
		4, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
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

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	PQclear(result);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int paquetChangePlaqueInscription(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	PGresult	*result;
	const char*	paramValues[4];
    Oid			paramTypes[4];
    int			paramLengths[4];
	int			paramFormats[4];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!minimumPayloadSize(paquet, sizeof(struct paquetPlaqueInscription))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetPlaqueInscription payload;

	inputBuffer = getData(inputBuffer, (char *)&payload, sizeof(payload));

	int expectedSize = sizeof(payload) + be32toh(payload.inscriptionLength);
	if (!expectedPayloadSize(paquet, expectedSize)) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	char *inscription = (char *)malloc(payload.inscriptionLength);
	if (inscription == NULL) {
#ifdef DEBUG
		fprintf(stderr, "Cannot allocate %ud bytes for inscription\n", payload.inscriptionLength);
#endif
		setTaskStatus(task, TaskStatusOutOfMemory);
		return -1;
	}

	inputBuffer = getData(inputBuffer, inscription, payload.inscriptionLength);

	struct dbh *dbh = peekDB(DB_PLAQUES_SESSION);
	if (dbh == NULL) {
		free(inscription);
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(uint64_t);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&task->profileId;
	paramTypes    [1] = INT8OID;
	paramLengths  [1] = sizeof(uint64_t);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&payload.plaqueToken;
	paramTypes    [2] = UUIDOID;
	paramLengths  [2] = TokenBinarySize;
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)inscription;
	paramTypes    [3] = TEXTOID;
	paramLengths  [3] = payload.inscriptionLength;
	paramFormats  [3] = 0;

	result = PQexecParams(dbh->conn, "UPDATE surrounding.plaques SET inscription = $4 WHERE plaque_token = $3 RETURNING plaque_token",
		4, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		pokeDB(dbh);
		free(inscription);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 1)) {
		PQclear(result);
		pokeDB(dbh);
		free(inscription);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfRows(result, 1)) {
		PQclear(result);
		pokeDB(dbh);
		free(inscription);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		pokeDB(dbh);
		free(inscription);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	PQclear(result);

	pokeDB(dbh);

	free(inscription);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}
