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

int
paquetDownloadPlaques(struct paquet *paquet)
{
	struct task	*task = paquet->task;

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

	outputBuffer = peekBufferOfSize(task->desk->pools.dynamic, KB, BUFFER_PLAQUES);
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

        dbhPushUUID(dbh, plaqueToken);

	    dbhExecute(dbh, "\
SELECT plaque_revision, profile_token, dimension, latitude, longitude, altitude, direction, tilt, width, height, background_color, foreground_color, font_size, inscription \
FROM surrounding.plaques \
JOIN auth.profiles USING (profile_id) \
WHERE plaque_token = $1");

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
		    reportLog("Requested plaque not found");
			continue;
//			pokeDB(dbh);
//			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
//			return -1;
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
		char directed			= PQgetisnull(dbh->result, 0, 6) ? 0 : 1;
		char *direction			= PQgetvalue(dbh->result, 0, 6);
		char tilted				= PQgetisnull(dbh->result, 0, 7) ? 0 : 1;
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

        uint32 plaqueStrobe = PaquetPlaqueStrobe;
		outputBuffer = putUInt32(outputBuffer, &plaqueStrobe);

		outputBuffer = putData(outputBuffer, plaqueToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, plaqueRevision, sizeof(uint32));
		outputBuffer = putData(outputBuffer, profileToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, dimension, 2 * sizeof(char));
		outputBuffer = putData(outputBuffer, latitude, sizeof(double));
		outputBuffer = putData(outputBuffer, longitude, sizeof(double));
		outputBuffer = putData(outputBuffer, altitude, sizeof(float));
		outputBuffer = putUInt8(outputBuffer, &directed);
		outputBuffer = putData(outputBuffer, direction, sizeof(float));
		outputBuffer = putUInt8(outputBuffer, &tilted);
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
