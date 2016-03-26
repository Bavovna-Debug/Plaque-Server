#include <c.h>
#include <string.h>

#include "db.h"
#include "buffers.h"
#include "paquet.h"
#include "report.h"
#include "reports.h"
#include "tasks.h"

int
reportMessage(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	const char	*paramValues[2];
    Oid			paramTypes[2];
    int			paramLengths[2];
	int			paramFormats[2];

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!minimumPayloadSize(paquet, sizeof(struct paquetReport))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetReport payload;

	inputBuffer = getData(inputBuffer, (char *)&payload, sizeof(payload));

	int expectedSize = sizeof(payload) + be32toh(payload.messageLength);
	if (!expectedPayloadSize(paquet, expectedSize)) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	char *message = (char *)malloc(payload.messageLength);
	if (message == NULL) {
		reportError("Cannot allocate %ud bytes for report message",
				payload.messageLength);
		setTaskStatus(task, TaskStatusOutOfMemory);
		return -1;
	}

	inputBuffer = getData(inputBuffer, message, payload.messageLength);

	struct dbh *dbh = peekDB(task->desk->dbh.guardian);
	if (dbh == NULL) {
		free(message);
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	paramValues   [0] = (char *)&task->deviceId;
	paramTypes    [0] = INT8OID;
	paramLengths  [0] = sizeof(uint64);
	paramFormats  [0] = 1;

	paramValues   [1] = (char *)message;
	paramTypes    [1] = TEXTOID;
	paramLengths  [1] = payload.messageLength;
	paramFormats  [1] = 0;

	dbh->result = PQexecParams(dbh->conn, "\
INSERT INTO debug.reports (device_id, message) \
VALUES ($1, $2)",
		2, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhCommandOK(dbh, dbh->result)) {
		pokeDB(dbh);
		free(message);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

	pokeDB(dbh);

	free(message);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}
