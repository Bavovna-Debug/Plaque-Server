#include <c.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "chalkboard.h"
#include "db.h"
#include "mmps.h"
#include "paquet.h"
#include "plaques_edit.h"
#include "report.h"
#include "tasks.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

int
HandlePostNewPlaque(struct Paquet *paquet)
{
#define QUERY_POST_NEW_PLAQUE "\
INSERT INTO surrounding.plaques ( \
    device_id, profile_id, dimension, latitude, longitude, altitude, \
    direction, tilt, width, height, \
    background_color, foreground_color, font_size, inscription) \
VALUES ($1, $2, '3D', $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13) \
RETURNING plaque_token"

	struct Task	*task = paquet->task;

	const char	*paramValues[13];
    Oid			paramTypes[13];
    int			paramLengths[13];
	int			paramFormats[13];

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!MinimumPayloadSize(paquet, sizeof(struct PaquetPostPlaque))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetPostPlaque plaque;

	inputBuffer = MMPS_GetData(inputBuffer, (char *) &plaque, sizeof(plaque), NULL);

	int expectedSize = sizeof(plaque) + be32toh(plaque.inscriptionLength);
	if (!ExpectedPayloadSize(paquet, expectedSize)) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	char *inscription = (char *) malloc(plaque.inscriptionLength);
	if (inscription == NULL) {
		ReportError("Cannot allocate %ud bytes for inscription",
				plaque.inscriptionLength);
		SetTaskStatus(task, TaskStatusOutOfMemory);
		return -1;
	}

	inputBuffer = MMPS_GetData(inputBuffer, inscription, plaque.inscriptionLength, NULL);

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		free(inscription);
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *) &task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->deviceId);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *) &task->profileId;
	paramTypes    [1] = INT8OID;
	paramLengths  [1] = sizeof(task->profileId);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *) &plaque.latitude;
	paramTypes    [2] = FLOAT8OID;
	paramLengths  [2] = sizeof(plaque.latitude);
	paramFormats  [2] = 1;

	paramValues   [3] = (char *) &plaque.longitude;
	paramTypes    [3] = FLOAT8OID;
	paramLengths  [3] = sizeof(plaque.longitude);
	paramFormats  [3] = 1;

	paramValues   [4] = (char *) &plaque.altitude;
	paramTypes    [4] = FLOAT4OID;
	paramLengths  [4] = sizeof(plaque.altitude);
	paramFormats  [4] = 1;

	if (plaque.directed == API_DeviceBooleanFalse) {
		paramValues   [5] = NULL;
		paramTypes    [5] = FLOAT4OID;
		paramLengths  [5] = 0;
		paramFormats  [5] = 1;
	} else {
		paramValues   [5] = (char *) &plaque.direction;
		paramTypes    [5] = FLOAT4OID;
		paramLengths  [5] = sizeof(plaque.direction);
		paramFormats  [5] = 1;
	}

	if (plaque.tilted == API_DeviceBooleanFalse) {
		paramValues   [6] = NULL;
		paramTypes    [6] = FLOAT4OID;
		paramLengths  [6] = 0;
		paramFormats  [6] = 1;
	} else {
		paramValues   [6] = (char *) &plaque.tilt;
		paramTypes    [6] = FLOAT4OID;
		paramLengths  [6] = sizeof(plaque.tilt);
		paramFormats  [6] = 1;
	}

	paramValues   [7] = (char *) &plaque.width;
	paramTypes    [7] = FLOAT4OID;
	paramLengths  [7] = sizeof(float);
	paramFormats  [7] = 1;

	paramValues   [8] = (char *) &plaque.height;
	paramTypes    [8] = FLOAT4OID;
	paramLengths  [8] = sizeof(plaque.height);
	paramFormats  [8] = 1;

	paramValues   [9] = (char *) &plaque.backgroundColor;
	paramTypes    [9] = INT4OID;
	paramLengths  [9] = sizeof(plaque.backgroundColor);
	paramFormats  [9] = 1;

	paramValues  [10] = (char *) &plaque.foregroundColor;
	paramTypes   [10] = INT4OID;
	paramLengths [10] = sizeof(plaque.foregroundColor);
	paramFormats [10] = 1;

	paramValues  [11] = (char *) &plaque.fontSize;
	paramTypes   [11] = FLOAT4OID;
	paramLengths [11] = sizeof(plaque.fontSize);
	paramFormats [11] = 1;

	paramValues  [12] = (char *) inscription;
	paramTypes   [12] = TEXTOID;
	paramLengths [12] = plaque.inscriptionLength;
	paramFormats [12] = 0;

	dbh->result = PQexecParams(dbh->conn, QUERY_POST_NEW_PLAQUE,
		13, paramTypes, paramValues, paramLengths, paramFormats, 1);

	free(inscription);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

	uint32 bonjourStatus = API_PaquetCreatePlaqueSucceeded;

	outputBuffer = MMPS_PutInt32(outputBuffer, &bonjourStatus);

	char *plaqueToken = PQgetvalue(dbh->result, 0, 0);

	outputBuffer = MMPS_PutData(outputBuffer, plaqueToken, API_TokenBinarySize);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
HandleChangePlaqueLocation(struct Paquet *paquet)
{
#define QUERY_CHANGE_PLAQUE_LOCATION "\
UPDATE surrounding.plaques \
SET latitude = $2, \
	longitude = $3, \
	altitude = $4 \
WHERE plaque_token = $1 \
RETURNING plaque_token"

	struct Task	*task = paquet->task;

	const char	*paramValues[4];
    Oid			paramTypes[4];
    int			paramLengths[4];
	int			paramFormats[4];

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!ExpectedPayloadSize(paquet, sizeof(struct PaquetPlaqueLocation))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetPlaqueLocation payload;

	inputBuffer = MMPS_GetData(inputBuffer, (char *) &payload, sizeof(payload), NULL);

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *) &payload.plaqueToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = API_TokenBinarySize;
	paramFormats  [0] = 1;

	paramValues   [1] = (char *) &payload.latitude;
	paramTypes    [1] = FLOAT8OID;
	paramLengths  [1] = sizeof(double);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *) &payload.longitude;
	paramTypes    [2] = FLOAT8OID;
	paramLengths  [2] = sizeof(double);
	paramFormats  [2] = 1;

	paramValues   [3] = (char *) &payload.altitude;
	paramTypes    [3] = FLOAT4OID;
	paramLengths  [3] = sizeof(float);
	paramFormats  [3] = 1;

	dbh->result = PQexecParams(dbh->conn, QUERY_CHANGE_PLAQUE_LOCATION,
		4, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
HandleChangePlaqueOrientation(struct Paquet *paquet)
{
#define QUERY_CHANGE_PLAQUE_ORIENTATION "\
UPDATE surrounding.plaques \
SET direction = $2, \
	tilt = $3 \
WHERE plaque_token = $1 \
RETURNING plaque_token"

	struct Task	*task = paquet->task;

	const char	*paramValues[3];
    Oid			paramTypes[3];
    int			paramLengths[3];
	int			paramFormats[3];

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!ExpectedPayloadSize(paquet, sizeof(struct PaquetPlaqueOrientation))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetPlaqueOrientation payload;

	inputBuffer = MMPS_GetData(inputBuffer, (char *) &payload, sizeof(payload), NULL);

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *) &payload.plaqueToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = API_TokenBinarySize;
	paramFormats  [0] = 1;

	if (payload.directed == API_DeviceBooleanFalse) {
		paramValues   [1] = NULL;
		paramTypes    [1] = FLOAT4OID;
		paramLengths  [1] = 0;
		paramFormats  [1] = 1;
	} else {
		paramValues   [1] = (char *) &payload.direction;
		paramTypes    [1] = FLOAT4OID;
		paramLengths  [1] = sizeof(payload.direction);
		paramFormats  [1] = 1;
	}

	if (payload.tilted == API_DeviceBooleanFalse) {
		paramValues   [2] = NULL;
		paramTypes    [2] = FLOAT4OID;
		paramLengths  [2] = 0;
		paramFormats  [2] = 1;
	} else {
		paramValues   [2] = (char *) &payload.tilt;
		paramTypes    [2] = FLOAT4OID;
		paramLengths  [2] = sizeof(payload.tilt);
		paramFormats  [2] = 1;
	}

	dbh->result = PQexecParams(dbh->conn, QUERY_CHANGE_PLAQUE_ORIENTATION,
		3, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
HandleChangePlaqueSize(struct Paquet *paquet)
{
#define QUERY_CHANGE_PLAQUE_SIZE "\
UPDATE surrounding.plaques \
SET width = $2, \
	height = $3 \
WHERE plaque_token = $1 \
RETURNING plaque_token"

	struct Task	*task = paquet->task;

	const char	*paramValues[3];
    Oid			paramTypes[3];
    int			paramLengths[3];
	int			paramFormats[3];

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!ExpectedPayloadSize(paquet, sizeof(struct PaquetPlaqueSize))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetPlaqueSize payload;

	inputBuffer = MMPS_GetData(inputBuffer, (char *) &payload, sizeof(payload), NULL);

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *) &payload.plaqueToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = API_TokenBinarySize;
	paramFormats  [0] = 1;

	paramValues   [1] = (char *) &payload.width;
	paramTypes    [1] = FLOAT4OID;
	paramLengths  [1] = sizeof(payload.width);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *) &payload.height;
	paramTypes    [2] = FLOAT4OID;
	paramLengths  [2] = sizeof(payload.height);
	paramFormats  [2] = 1;

	dbh->result = PQexecParams(dbh->conn, QUERY_CHANGE_PLAQUE_SIZE,
		3, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
HandleChangePlaqueColors(struct Paquet *paquet)
{
#define QUERY_CHANGE_PLAQUE_COLORS "\
UPDATE surrounding.plaques \
SET background_color = $2, \
	foreground_color = $3 \
WHERE plaque_token = $1 \
RETURNING plaque_token"

	struct Task	*task = paquet->task;

	const char	*paramValues[3];
    Oid			paramTypes[3];
    int			paramLengths[3];
	int			paramFormats[3];

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!ExpectedPayloadSize(paquet, sizeof(struct PaquetPlaqueColors))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetPlaqueColors payload;

	inputBuffer = MMPS_GetData(inputBuffer, (char *) &payload, sizeof(payload), NULL);

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *) &payload.plaqueToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = API_TokenBinarySize;
	paramFormats  [0] = 1;

	paramValues   [1] = (char *) &payload.backgroundColor;
	paramTypes    [1] = INT4OID;
	paramLengths  [1] = sizeof(payload.backgroundColor);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *) &payload.foregroundColor;
	paramTypes    [2] = INT4OID;
	paramLengths  [2] = sizeof(payload.foregroundColor);
	paramFormats  [2] = 1;

	dbh->result = PQexecParams(dbh->conn, QUERY_CHANGE_PLAQUE_COLORS,
		3, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
HandleChangePlaqueFont(struct Paquet *paquet)
{
#define QUERY_CHANGE_PLAQUE_FONT "\
UPDATE surrounding.plaques \
SET font_size = $2 \
WHERE plaque_token = $1 \
RETURNING plaque_token"

	struct Task	*task = paquet->task;

	const char	*paramValues[2];
    Oid			paramTypes[2];
    int			paramLengths[2];
	int			paramFormats[2];

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!ExpectedPayloadSize(paquet, sizeof(struct PaquetPlaqueFont))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetPlaqueFont payload;

	inputBuffer = MMPS_GetData(inputBuffer, (char *) &payload, sizeof(payload), NULL);

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *) &payload.plaqueToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = API_TokenBinarySize;
	paramFormats  [0] = 1;

	paramValues   [1] = (char *) &payload.fontSize;
	paramTypes    [1] = FLOAT4OID;
	paramLengths  [1] = sizeof(payload.fontSize);
	paramFormats  [1] = 1;

	dbh->result = PQexecParams(dbh->conn, QUERY_CHANGE_PLAQUE_FONT,
		2, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
HandleChangePlaqueInscription(struct Paquet *paquet)
{
#define QUERY_CHANGE_PLAQUE_INSCRIPTION "\
UPDATE surrounding.plaques \
SET inscription = $2 \
WHERE plaque_token = $1 \
RETURNING plaque_token"

	struct Task	*task = paquet->task;

	const char	*paramValues[2];
    Oid			paramTypes[2];
    int			paramLengths[2];
	int			paramFormats[2];

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!MinimumPayloadSize(paquet, sizeof(struct PaquetPlaqueInscription))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetPlaqueInscription payload;

	inputBuffer = MMPS_GetData(inputBuffer, (char *) &payload, sizeof(payload), NULL);

	int expectedSize = sizeof(payload) + be32toh(payload.inscriptionLength);
	if (!ExpectedPayloadSize(paquet, expectedSize)) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	char *inscription = (char *) malloc(payload.inscriptionLength);
	if (inscription == NULL) {
		ReportError("Cannot allocate %ud bytes for inscription",
				payload.inscriptionLength);
		SetTaskStatus(task, TaskStatusOutOfMemory);
		return -1;
	}

	inputBuffer = MMPS_GetData(inputBuffer, inscription, payload.inscriptionLength, NULL);

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		free(inscription);
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *) &payload.plaqueToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = API_TokenBinarySize;
	paramFormats  [0] = 1;

	paramValues   [1] = (char *) inscription;
	paramTypes    [1] = TEXTOID;
	paramLengths  [1] = payload.inscriptionLength;
	paramFormats  [1] = 0;

	dbh->result = PQexecParams(dbh->conn, QUERY_CHANGE_PLAQUE_INSCRIPTION,
		2, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		free(inscription);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		free(inscription);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfRows(dbh->result, 1)) {
		DB_PokeHandle(dbh);
		free(inscription);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		free(inscription);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

	DB_PokeHandle(dbh);

	free(inscription);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}
