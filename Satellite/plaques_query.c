#include <c.h>
#include <string.h>

#include "api.h"
#include "db.h"
#include "buffers.h"
#include "desk.h"
#include "paquet.h"
#include "plaques_query.h"
#include "report.h"
#include "tasks.h"

void
journalUserLocation(
    struct paquet *paquet,
    struct dbh *dbh,
    struct paquetRadar *radar)
{
	struct task	*task = paquet->task;

	const char	*paramValues[6];
    Oid			paramTypes[6];
    int			paramLengths[6];
	int			paramFormats[6];

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(uint64);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&radar->latitude;
	paramTypes    [1] = FLOAT8OID;
	paramLengths  [1] = sizeof(double);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&radar->longitude;
	paramTypes    [2] = FLOAT8OID;
	paramLengths  [2] = sizeof(double);
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)&radar->altitude;
	paramTypes    [3] = FLOAT4OID;
	paramLengths  [3] = sizeof(float);
	paramFormats  [3] = 1;

	if (radar->courseAvailable == DeviceBooleanFalse) {
		paramValues   [4] = NULL;
		paramTypes    [4] = FLOAT4OID;
		paramLengths  [4] = 0;
		paramFormats  [4] = 1;
	} else {
		paramValues   [4] = (char *)&radar->course;
		paramTypes    [4] = FLOAT4OID;
		paramLengths  [4] = sizeof(float);
		paramFormats  [4] = 1;
	}

	if (radar->floorLevelAvailable == DeviceBooleanFalse) {
		paramValues   [5] = NULL;
		paramTypes    [5] = INT4OID;
		paramLengths  [5] = 0;
		paramFormats  [5] = 1;
	} else {
		paramValues   [5] = (char *)&radar->floorLevel;
		paramTypes    [5] = INT4OID;
		paramLengths  [5] = sizeof(int32_t);
		paramFormats  [5] = 1;
	}

	dbh->result = PQexecParams(dbh->conn, "\
INSERT INTO journal.movements \
(device_id, latitude, longitude, altitude, course, floor_level) \
VALUES ($1, $2, $3, $4, $5, $6)",
		6, paramTypes, paramValues, paramLengths, paramFormats, 1);
}

int
paquetListOfPlaquesForBroadcast(struct paquet *paquet)
{
	return 0;
}

int
paquetListOfPlaquesOnRadar(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	const char	*paramValues[5];
    Oid			paramTypes[5];
    int			paramLengths[5];
	int			paramFormats[5];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetRadar))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 nextOnRadarRevision;
    if (getSessionNextOnRadarRevision(task, dbh, &nextOnRadarRevision)) {
		pokeDB(dbh);
		return -1;
	}

	struct paquetRadar *radar = (struct paquetRadar *)inputBuffer->cursor;

	paramValues   [0] = (char *)&task->sessionId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->sessionId);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&radar->radarRevision;
	paramTypes    [1] = INT4OID;
	paramLengths  [1] = sizeof(radar->radarRevision);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&radar->latitude;
	paramTypes    [2] = FLOAT8OID;
	paramLengths  [2] = sizeof(radar->latitude);
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)&radar->longitude;
	paramTypes    [3] = FLOAT8OID;
	paramLengths  [3] = sizeof(radar->longitude);
	paramFormats  [3] = 1;

	paramValues   [4] = (char *)&radar->range;
	paramTypes    [4] = FLOAT4OID;
	paramLengths  [4] = sizeof(radar->range);
	paramFormats  [4] = 1;

	dbh->result = PQexecParams(dbh->conn, "\
SELECT plaque_token, plaque_revision \
FROM surrounding.query_plaques_on_radar($1, $2, $3, $4, $5)",
		5, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 2)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, UUIDOID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 1, INT4OID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	outputBuffer = putData(outputBuffer, (char *)&nextOnRadarRevision, sizeof(nextOnRadarRevision));

	uint32 numberOfPlaques = PQntuples(dbh->result);

	reportLog("Found %d plaques", numberOfPlaques);

	outputBuffer = putUInt32(outputBuffer, &numberOfPlaques);

	int rowNumber;
	for (rowNumber = 0; rowNumber < numberOfPlaques; rowNumber++)
	{
		char *plaqueToken = PQgetvalue(dbh->result, rowNumber, 0);
		char *plaqueRevision = PQgetvalue(dbh->result, rowNumber, 1);
		if ((plaqueToken == NULL) || (plaqueRevision == NULL)) {
			reportError("No results");
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = putData(outputBuffer, plaqueToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, plaqueRevision, sizeof(uint32));
	}

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
paquetListOfPlaquesInSight(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	const char	*paramValues[5];
    Oid			paramTypes[5];
    int			paramLengths[5];
	int			paramFormats[5];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetRadar))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 nextInSightRevision;
    if (getSessionNextInSightRevision(task, dbh, &nextInSightRevision)) {
		pokeDB(dbh);
		return -1;
	}

	struct paquetRadar *radar = (struct paquetRadar *)inputBuffer->cursor;

	journalUserLocation(paquet, dbh, radar);

	paramValues   [0] = (char *)&task->sessionId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(task->sessionId);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&radar->radarRevision;
	paramTypes    [1] = INT4OID;
	paramLengths  [1] = sizeof(radar->radarRevision);
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&radar->latitude;
	paramTypes    [2] = FLOAT8OID;
	paramLengths  [2] = sizeof(radar->latitude);
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)&radar->longitude;
	paramTypes    [3] = FLOAT8OID;
	paramLengths  [3] = sizeof(radar->longitude);
	paramFormats  [3] = 1;

	paramValues   [4] = (char *)&radar->range;
	paramTypes    [4] = FLOAT4OID;
	paramLengths  [4] = sizeof(radar->range);
	paramFormats  [4] = 1;

	dbh->result = PQexecParams(dbh->conn, "\
SELECT plaque_token, plaque_revision \
FROM surrounding.query_plaques_in_sight($1, $2, $3, $4, $5)",
		5, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 2)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 0, UUIDOID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectColumnType(dbh->result, 1, INT4OID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	outputBuffer = putData(outputBuffer, (char *)&nextInSightRevision, sizeof(nextInSightRevision));

	uint32 numberOfPlaques = PQntuples(dbh->result);

	reportLog("Found %d plaques", numberOfPlaques);

	outputBuffer = putUInt32(outputBuffer, &numberOfPlaques);

	int rowNumber;
	for (rowNumber = 0; rowNumber < numberOfPlaques; rowNumber++)
	{
		char *plaqueToken = PQgetvalue(dbh->result, rowNumber, 0);
		char *plaqueRevision = PQgetvalue(dbh->result, rowNumber, 1);
		if ((plaqueToken == NULL) || (plaqueRevision == NULL)) {
			reportError("No results");
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = putData(outputBuffer, plaqueToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, plaqueRevision, sizeof(uint32));
	}

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
paquetDownloadPlaques(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	const char	*paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = NULL;

	uint32 numberOfPlaques;

	if (!minimumPayloadSize(paquet, sizeof(numberOfPlaques))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	inputBuffer = getUInt32(inputBuffer, &numberOfPlaques);

	if (!expectedPayloadSize(paquet, sizeof(numberOfPlaques) + numberOfPlaques * TokenBinarySize)) {
		reportError("Wrong payload for %d plaques", numberOfPlaques);
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	outputBuffer = peekBufferOfSize(task->desk->pools.dynamic, 4 * KB);
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

	char plaqueToken[TokenBinarySize];
	int i;
	for (i = 0; i < numberOfPlaques; i++)
	{
		inputBuffer = getData(inputBuffer, plaqueToken, TokenBinarySize);

		paramValues   [0] = (char *)&plaqueToken;
		paramTypes    [0] = UUIDOID;
		paramLengths  [0] = TokenBinarySize;
		paramFormats  [0] = 1;

        if (dbh->result != NULL)
        	PQclear(dbh->result);

		dbh->result = PQexecParams(dbh->conn, "\
SELECT plaque_revision, profile_token, dimension, latitude, longitude, altitude, direction, tilt, width, height, background_color, foreground_color, font_size, inscription \
FROM surrounding.plaques \
JOIN auth.profiles USING (profile_id) \
WHERE plaque_token = $1",
			1, paramTypes, paramValues, paramLengths, paramFormats, 1);

		if (!dbhTuplesOK(dbh, dbh->result)) {
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		if (!dbhCorrectNumberOfColumns(dbh->result, 14)) {
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
			!dbhCorrectColumnType(dbh->result, 1, UUIDOID) ||
			//!dbhCorrectColumnType(dbh->result, 2, CHAROID) ||
			!dbhCorrectColumnType(dbh->result, 3, FLOAT8OID) ||
			!dbhCorrectColumnType(dbh->result, 4, FLOAT8OID) ||
			!dbhCorrectColumnType(dbh->result, 5, FLOAT4OID) ||
			!dbhCorrectColumnType(dbh->result, 6, FLOAT4OID) ||
			!dbhCorrectColumnType(dbh->result, 7, FLOAT4OID) ||
			!dbhCorrectColumnType(dbh->result, 8, FLOAT4OID) ||
			!dbhCorrectColumnType(dbh->result, 9, FLOAT4OID) ||
			!dbhCorrectColumnType(dbh->result, 10, INT4OID) ||
			!dbhCorrectColumnType(dbh->result, 11, INT4OID) ||
			!dbhCorrectColumnType(dbh->result, 12, FLOAT4OID) ||
			!dbhCorrectColumnType(dbh->result, 13, TEXTOID))
		{
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		char *plaqueRevision	= PQgetvalue(dbh->result, 0, 0);
		char *profileToken		= PQgetvalue(dbh->result, 0, 1);
		char *dimension			= PQgetvalue(dbh->result, 0, 2);
		char *latitude			= PQgetvalue(dbh->result, 0, 3);
		char *longitude			= PQgetvalue(dbh->result, 0, 4);
		char *altitude			= PQgetvalue(dbh->result, 0, 5);
		char directed			= PQgetisnull(dbh->result, 0, 6) ? '0' : '1';
		char *direction			= PQgetvalue(dbh->result, 0, 6);
		char tilted				= PQgetisnull(dbh->result, 0, 7) ? '0' : '1';
		char *tilt				= PQgetvalue(dbh->result, 0, 7);
		char *width				= PQgetvalue(dbh->result, 0, 8);
		char *height			= PQgetvalue(dbh->result, 0, 9);
		char *backgroundColor	= PQgetvalue(dbh->result, 0, 10);
		char *foregroundColor	= PQgetvalue(dbh->result, 0, 11);
		char *fontSize			= PQgetvalue(dbh->result, 0, 12);
		char *inscription		= PQgetvalue(dbh->result, 0, 13);
		int inscriptionSize		= PQgetlength(dbh->result, 0, 13);

		if ((plaqueRevision == NULL) || (profileToken == NULL)
			|| (dimension == NULL) || (latitude == NULL) || (longitude == NULL) || (altitude == NULL)
			|| (direction == NULL) || (tilt == NULL) || (width == NULL) || (height == NULL)
			|| (backgroundColor == NULL) || (foregroundColor == NULL) || (fontSize == NULL)
			|| (inscription == NULL)) {
			reportError("No results");
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = putData(outputBuffer, plaqueToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, plaqueRevision, sizeof(uint32));
		outputBuffer = putData(outputBuffer, profileToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, dimension, 2 * sizeof(char));
		outputBuffer = putData(outputBuffer, latitude, sizeof(double));
		outputBuffer = putData(outputBuffer, longitude, sizeof(double));
		outputBuffer = putData(outputBuffer, altitude, sizeof(float));
		outputBuffer = putUInt8(outputBuffer, directed);
		outputBuffer = putData(outputBuffer, direction, sizeof(float));
		outputBuffer = putUInt8(outputBuffer, tilted);
		outputBuffer = putData(outputBuffer, tilt, sizeof(float));
		outputBuffer = putData(outputBuffer, width, sizeof(float));
		outputBuffer = putData(outputBuffer, height, sizeof(float));
		outputBuffer = putData(outputBuffer, backgroundColor, sizeof(uint32));
		outputBuffer = putData(outputBuffer, foregroundColor, sizeof(uint32));
		outputBuffer = putData(outputBuffer, fontSize, sizeof(float));
		outputBuffer = putString(outputBuffer, inscription, inscriptionSize);
	}

	pokeDB(dbh);

	return 0;
}
