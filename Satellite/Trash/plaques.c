#include <string.h>
#include "api.h"
#include "bonjour.h"
#include "buffers.h"
#include "processor.h"

int getPlaquesForRadar(struct dbh *dbh, struct task *task)
{
	PGresult	*result;
	const char*	paramValues[3];
    Oid			paramTypes[3];
    int			paramLengths[3];
	int			paramFormats[3];

	if (deviceIdByToken(dbh, bonjourDeviceToken(task->request)) == 0)
		return -1;

	struct buffer *request = task->request;
	struct buffer *response = request;

	uint32_t payloadSize = bonjourGetPayloadSize(request);

	if (payloadSize != sizeof(struct bonjourRadar)) {
		fprintf(stderr, "Wrong payload size\n");
		return -1;
	}

	resetCursor(request, 1);

	struct bonjourRadar *radar = (struct bonjourRadar *)request->cursor;

	paramValues   [0] = (char *)&radar->latitude;
	paramTypes    [0] = FLOAT8OID;
	paramLengths  [0] = 8;
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&radar->longitude;
	paramTypes    [1] = FLOAT8OID;
	paramLengths  [1] = 8;
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&radar->range;
	paramTypes    [2] = FLOAT4OID;
	paramLengths  [2] = 4;
	paramFormats  [2] = 1;

	result = PQexecParams(dbh->conn, "SELECT (journal.query_plaques($1, $2, $3)).*",
		3, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(result, 2)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectColumnType(result, 0, UUIDOID)) {
		PQclear(result);
		return -1;
	}

	if (!dbhCorrectColumnType(result, 1, INT4OID)) {
		PQclear(result);
		return -1;
	}

	resetBufferData(response, 1);

	uint32_t numberOfRows = PQntuples(result);

	response = putUInt32(response, &numberOfRows);

	int rowNumber;
	for (rowNumber = 0; rowNumber < numberOfRows; rowNumber++)
	{
		char *plaqueToken = PQgetvalue(result, rowNumber, 0);
		char *plaqueRevision = PQgetvalue(result, rowNumber, 1);
		if ((plaqueToken == NULL) || (plaqueRevision == NULL)) {
			fprintf(stderr, "No results\n");
			PQclear(result);
			return -1;
		}

		response = putData(response, plaqueToken, TOKEN_SIZE);
		response = putData(response, plaqueRevision, sizeof(uint32_t));
	}

	PQclear(result);

	task->response = response;

	return 0;
}

int getPlaquesWithDetails(struct dbh *dbh, struct task *task)
{
	PGresult	*result;
	const char*	paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	if (deviceIdByToken(dbh, bonjourDeviceToken(task->request)) == 0)
		return -1;

	struct buffer *request = task->request;
	struct buffer *response = NULL;

	uint32_t payloadSize = bonjourGetPayloadSize(request);

	uint32_t numberOfPlaques;

	if (payloadSize < sizeof(numberOfPlaques)) {
		fprintf(stderr, "Wrong payload size %d\n", payloadSize);
		return -1;
	}

	resetCursor(request, 1);

	request = getUInt32(request, &numberOfPlaques);

	if (payloadSize != sizeof(numberOfPlaques) + numberOfPlaques * TOKEN_SIZE) {
		fprintf(stderr, "Wrong payload size %d\n", payloadSize);
		return -1;
	}

	task->response = peekBuffer(BUFFER4K);
	if (task->response == NULL) {
		printf("No memory\n");
		return -1;
	}

	response = task->response;

	bonjourCopyPilot(task->response, task->request);

	resetBufferData(response, 1);

	int i;
	for (i = 0; i < numberOfPlaques; i++)
	{
		char plaqueToken[TOKEN_SIZE];

		request = getData(request, plaqueToken, TOKEN_SIZE);

		paramValues   [0] = (char *)&plaqueToken;
		paramTypes    [0] = UUIDOID;
		paramLengths  [0] = TOKEN_SIZE;
		paramFormats  [0] = 1;

		result = PQexecParams(dbh->conn, "SELECT revision, latitude, longitude, altitude, direction, width, height, background_color, foreground_color, inscription FROM journal.plaques WHERE plaque_token = $1",
			1, paramTypes, paramValues, paramLengths, paramFormats, 1);

		if (!dbhTuplesOK(dbh, result)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectNumberOfColumns(result, 10)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectNumberOfRows(result, 1)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 0, INT4OID)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 1, FLOAT8OID)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 2, FLOAT8OID)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 3, FLOAT4OID)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 4, FLOAT4OID)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 5, FLOAT4OID)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 6, FLOAT4OID)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 7, INT4OID)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 8, INT4OID)) {
			PQclear(result);
			return -1;
		}

		if (!dbhCorrectColumnType(result, 9, TEXTOID)) {
			PQclear(result);
			return -1;
		}

		char *revision = PQgetvalue(result, 0, 0);
		char *latitude = PQgetvalue(result, 0, 1);
		char *longitude = PQgetvalue(result, 0, 2);
		char *altitude = PQgetvalue(result, 0, 3);
		int directed = PQgetisnull(result, 0, 4);
		char *direction = PQgetvalue(result, 0, 4);
		char *width = PQgetvalue(result, 0, 5);
		char *height = PQgetvalue(result, 0, 6);
		char *backgroundColor = PQgetvalue(result, 0, 7);
		char *foregroundColor = PQgetvalue(result, 0, 8);
		char *inscription = PQgetvalue(result, 0, 9);
		int inscriptionSize = PQgetlength(result, 0, 9);

		if ((revision == NULL) || (latitude == NULL) || (longitude == NULL) || (altitude == NULL) || (direction == NULL)
			|| (width == NULL) || (height == NULL) || (backgroundColor == NULL) || (foregroundColor == NULL)
			|| (inscription == NULL)) {
			fprintf(stderr, "No results\n");
			PQclear(result);
			return -1;
		}

		response = putData(response, plaqueToken, TOKEN_SIZE);
		response = putData(response, revision, sizeof(uint32_t));
		response = putData(response, latitude, sizeof(double));
		response = putData(response, longitude, sizeof(double));
		response = putData(response, altitude, sizeof(float));
		response = putUInt8(response, directed);
		response = putData(response, direction, sizeof(float));
		response = putData(response, width, sizeof(float));
		response = putData(response, height, sizeof(float));
		response = putData(response, backgroundColor, sizeof(uint32_t));
		response = putData(response, foregroundColor, sizeof(uint32_t));
		response = putString(response, inscription, inscriptionSize);

		PQclear(result);
	}

	return 0;
}

int createPlaque(struct dbh *dbh, struct task *task)
{
	PGresult	*result;
	const char*	paramValues[11];
    Oid			paramTypes[11];
    int			paramLengths[11];
	int			paramFormats[11];

	uint64_t deviceIdBigEndian = deviceIdByToken(dbh, bonjourDeviceToken(task->request));
	if (deviceIdBigEndian == 0)
		return -1;

	struct buffer *request = task->request;
	struct buffer *response = request;

	uint32_t payloadSize = bonjourGetPayloadSize(request);

	if (payloadSize < sizeof(struct bonjourNewPlaque)) {
		fprintf(stderr, "Wrong payload size %d, expected minimum %lu\n", payloadSize, sizeof(struct bonjourNewPlaque));
		return -1;
	}

	resetCursor(request, 1);

	struct bonjourNewPlaque *plaque = (struct bonjourNewPlaque *)request->cursor;

	int expectedPayloadSize = sizeof(struct bonjourNewPlaque) + be32toh(plaque->inscriptionLength);
	if (payloadSize < expectedPayloadSize) {
		fprintf(stderr, "Wrong payload size %d, expected %d\n", payloadSize, expectedPayloadSize);
		return -1;
	}

	paramValues   [0] = (char *)&deviceIdBigEndian;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = 8;
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)&plaque->latitude;
	paramTypes    [1] = FLOAT8OID;
	paramLengths  [1] = 8;
	paramFormats  [1] = 1;

	paramValues   [2] = (char *)&plaque->longitude;
	paramTypes    [2] = FLOAT8OID;
	paramLengths  [2] = 8;
	paramFormats  [2] = 1;

	paramValues   [3] = (char *)&plaque->altitude;
	paramTypes    [3] = FLOAT4OID;
	paramLengths  [3] = 4;
	paramFormats  [3] = 1;

	char directed = plaque->directed;
	convertBooleanToPostgres(&directed);
	paramValues   [4] = (char *)&directed;
	paramTypes    [4] = BOOLOID;
	paramLengths  [4] = 1;
	paramFormats  [4] = 1;

	if (directed == 0) {
		paramValues   [5] = NULL;
		paramTypes    [5] = FLOAT4OID;
		paramLengths  [5] = 0;
		paramFormats  [5] = 1;
	} else {
		paramValues   [5] = (char *)&plaque->direction;
		paramTypes    [5] = FLOAT4OID;
		paramLengths  [5] = 4;
		paramFormats  [5] = 1;
	}

	paramValues   [6] = (char *)&plaque->width;
	paramTypes    [6] = FLOAT4OID;
	paramLengths  [6] = 4;
	paramFormats  [6] = 1;

	paramValues   [7] = (char *)&plaque->height;
	paramTypes    [7] = FLOAT4OID;
	paramLengths  [7] = 4;
	paramFormats  [7] = 1;

	paramValues   [8] = (char *)&plaque->backgroundColor;
	paramTypes    [8] = INT4OID;
	paramLengths  [8] = 4;
	paramFormats  [8] = 1;

	paramValues   [9] = (char *)&plaque->foregroundColor;
	paramTypes    [9] = INT4OID;
	paramLengths  [9] = 4;
	paramFormats  [9] = 1;

	paramValues  [10] = (char *)&plaque->inscription;
	paramTypes   [10] = TEXTOID;
	paramLengths [10] = plaque->inscriptionLength;
	paramFormats [10] = 0;

	result = PQexecParams(dbh->conn, "INSERT INTO journal.plaques (device_id, kind_of_content, latitude, longitude, altitude, directed, direction, width, height, background_color, foreground_color, inscription) VALUES ($1, 'PLAINTEXT', $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) RETURNING plaque_token",
		11, paramTypes, paramValues, paramLengths, paramFormats, 1);

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

	uint32_t bonjourStatus = BonjourCreatePlaqueSucceeded;

	response = putUInt32(response, &bonjourStatus);

	char *plaqueToken = PQgetvalue(result, 0, 0);

	response = putData(response, plaqueToken, TOKEN_SIZE);

	PQclear(result);

	task->response = response;

	return 0;
}
