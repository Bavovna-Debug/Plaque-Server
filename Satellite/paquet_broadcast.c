#include <c.h>
#include <string.h>

#include "api.h"
#include "db.h"
#include "buffers.h"
#include "paquet.h"
#include "paquet_broadcast.h"
#include "report.h"
#include "tasks.h"

static int
paquetBroadcastPlaquesOnRadar(struct paquet *paquet);

static int
paquetBroadcastPlaquesInSight(struct paquet *paquet);

static int
paquetBroadcastPlaquesOnMap(struct paquet *paquet);

int
paquetBroadcast(struct paquet *paquet)
{
    struct task         *task;
    struct revisions    *lastKnownRevision;
    struct revisions    *currentRevision;
    struct revisions    missingRevision;
    int                 rc;

    task = paquet->task;

    lastKnownRevision = &task->broadcast.lastKnownRevision;
    currentRevision = &task->broadcast.currentRevision;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetBroadcast))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(paquet->inputBuffer, 1);

	struct paquetBroadcast *broadcast = (struct paquetBroadcast *)paquet->inputBuffer->cursor;

	lastKnownRevision->onRadar = be32toh(broadcast->lastKnownOnRadarRevision);
	lastKnownRevision->inSight = be32toh(broadcast->lastKnownInSightRevision);
	lastKnownRevision->onMap = be32toh(broadcast->lastKnownOnMapRevision);

    rc = getSessionRevisions(task, currentRevision);
    if (rc != 0) {
		setTaskStatus(task, TaskStatusOtherError);
		return -1;
    }

    reportLog("'On radar' revisions: 'last known' %u 'current' %u",
        lastKnownRevision->onRadar,
        currentRevision->onRadar);
    reportLog("'In sight' revisions: 'last known' %u 'current' %u",
        lastKnownRevision->inSight,
        currentRevision->inSight);
    reportLog("'On map' revisions: 'last known' %u 'current' %u",
        lastKnownRevision->onMap,
        currentRevision->onMap);

    if (lastKnownRevision->onRadar > currentRevision->onRadar)
        lastKnownRevision->onRadar = 0;

    if (lastKnownRevision->inSight > currentRevision->inSight)
        lastKnownRevision->inSight = 0;

    if (lastKnownRevision->onMap > currentRevision->onMap)
        lastKnownRevision->onMap = 0;

    pthread_mutex_lock(&task->broadcast.editMutex);

	if (task->broadcast.broadcastPaquet != NULL) {
		//paquetCancel(task->broadcast.broadcastPaquet);
	    pthread_mutex_unlock(&task->broadcast.editMutex);
		reportLog("Broadcast request received while another broadcast request is still in process");
	    return -1;
	}

	task->broadcast.broadcastPaquet = paquet;

	pthread_mutex_unlock(&task->broadcast.editMutex);

    missingRevision.onRadar = currentRevision->onRadar - lastKnownRevision->onRadar;
    missingRevision.inSight = currentRevision->inSight - lastKnownRevision->inSight;
    missingRevision.onMap = currentRevision->onMap - lastKnownRevision->onMap;

    if ((missingRevision.onRadar > 0) || (missingRevision.inSight > 0) || (missingRevision.onMap > 0)) {
        reportLog("Do not wait for boradcast because there are already %u / %u / %u missing revisions",
            missingRevision.onRadar,
            missingRevision.inSight,
            missingRevision.onMap);
    } else {
        reportLog("Waiting for broadcast with known revisions %u / %u / %u",
            lastKnownRevision->onRadar,
            lastKnownRevision->inSight,
            lastKnownRevision->onMap);

        rc = pthread_mutex_lock(&task->broadcast.waitMutex);
        if (rc != 0) {
            reportError("Error has occurred on mutex lock: rc=%d", rc);
            return -1;
        }

        rc = pthread_cond_wait(&task->broadcast.waitCondition, &task->broadcast.waitMutex);
        if (rc != 0) {
            pthread_mutex_unlock(&task->broadcast.waitMutex);
            reportError("Error has occurred while whaiting for condition: rc=%d", rc);
            return -1;
        }

        rc = pthread_mutex_unlock(&task->broadcast.waitMutex);
        if (rc != 0) {
            reportError("Error has occurred on mutex unlock: rc=%d", rc);
            return -1;
        }

        reportLog("Received broadcast");
    }

	pthread_mutex_lock(&task->broadcast.editMutex);

    if (currentRevision->onRadar > lastKnownRevision->onRadar) {
        reportLog("Fetch 'on radar' for broadcast from revision %u to %u",
            lastKnownRevision->onRadar,
            currentRevision->onRadar);
        rc = paquetBroadcastPlaquesOnRadar(paquet);
    } else if (currentRevision->inSight > lastKnownRevision->inSight) {
        reportLog("Fetch 'in sight' for broadcast from revision %u to %u",
            lastKnownRevision->inSight,
            currentRevision->inSight);
        rc = paquetBroadcastPlaquesInSight(paquet);
    } else if (currentRevision->onMap > lastKnownRevision->onMap) {
        reportLog("Fetch 'on map' for broadcast from revision %u to %u",
            lastKnownRevision->onMap,
            currentRevision->onMap);
        rc = paquetBroadcastPlaquesOnMap(paquet);
    } else {
        reportLog("Nothing to fetch for broadcast");
        rc = -1;
    }

	task->broadcast.broadcastPaquet = NULL;

	pthread_mutex_unlock(&task->broadcast.editMutex);

    return rc;
}

static int
paquetBroadcastPlaquesOnRadar(struct paquet *paquet)
{
	struct task	*task = paquet->task;

    struct buffer *outputBuffer = peekBufferOfSize(task->desk->pools.dynamic, 512, BUFFER_BROADCAST);
	if (outputBuffer == NULL) {
		setTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

    paquet->outputBuffer = outputBuffer;

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 currentRevision;
    if (getSessionOnRadarRevision(task, dbh, &currentRevision) != 0) {
		pokeDB(dbh);
		return -1;
	}

    uint32 lastKnownRevision = htobe32(task->broadcast.lastKnownRevision.onRadar);
    dbhPushBIGINT(dbh, &task->sessionId);
    dbhPushINTEGER(dbh, &lastKnownRevision);

	dbhExecute(dbh, "\
SELECT plaque_token, plaque_revision, disappeared \
FROM journal.session_on_radar_plaques \
JOIN surrounding.plaques \
    USING (plaque_id) \
WHERE session_id = $1 \
  AND on_radar_revision > $2");

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 3)) {
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

	if (!dbhCorrectColumnType(dbh->result, 2, BOOLOID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

    uint32 broadcastDestination = BroadcastDestinationOnRadar;
	outputBuffer = putUInt32(outputBuffer, &broadcastDestination);

	outputBuffer = putUInt32(outputBuffer, &currentRevision);

	uint32 numberOfPlaques = PQntuples(dbh->result);

	reportLog("Found %u plaques for 'on radar' current revision %u last known revision %u",
	    numberOfPlaques,
	    currentRevision,
	    task->broadcast.lastKnownRevision.onRadar);

	outputBuffer = putUInt32(outputBuffer, &numberOfPlaques);

	int rowNumber;
	for (rowNumber = 0; rowNumber < numberOfPlaques; rowNumber++)
	{
		char *plaqueToken = PQgetvalue(dbh->result, rowNumber, 0);
		char *plaqueRevision = PQgetvalue(dbh->result, rowNumber, 1);
		char disappeared = *PQgetvalue(dbh->result, rowNumber, 2);
		if ((plaqueToken == NULL) || (plaqueRevision == NULL)) {
			reportError("No results");
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = putData(outputBuffer, plaqueToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, plaqueRevision, sizeof(uint32));
		outputBuffer = putData(outputBuffer, &disappeared, sizeof(disappeared));
	}

	pokeDB(dbh);

	return 0;
}

static int
paquetBroadcastPlaquesInSight(struct paquet *paquet)
{
	struct task	*task = paquet->task;

    struct buffer *outputBuffer = peekBufferOfSize(task->desk->pools.dynamic, 512, BUFFER_BROADCAST);
	if (outputBuffer == NULL) {
		setTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

    paquet->outputBuffer = outputBuffer;

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 currentRevision;
    if (getSessionInSightRevision(task, dbh, &currentRevision) != 0) {
		pokeDB(dbh);
		return -1;
	}

    uint32 lastKnownRevision = htobe32(task->broadcast.lastKnownRevision.inSight);
    dbhPushBIGINT(dbh, &task->sessionId);
    dbhPushINTEGER(dbh, &lastKnownRevision);

	dbhExecute(dbh, "\
SELECT plaque_token, plaque_revision, disappeared \
FROM journal.session_in_sight_plaques \
JOIN surrounding.plaques \
    USING (plaque_id) \
WHERE session_id = $1 \
  AND in_sight_revision > $2");

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 3)) {
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

	if (!dbhCorrectColumnType(dbh->result, 2, BOOLOID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

    uint32 broadcastDestination = BroadcastDestinationInSight;
	outputBuffer = putUInt32(outputBuffer, &broadcastDestination);

	outputBuffer = putUInt32(outputBuffer, &currentRevision);

	uint32 numberOfPlaques = PQntuples(dbh->result);

	reportLog("Found %u plaques for 'in sight' current revision %u last known revision %u",
	    numberOfPlaques,
	    currentRevision,
	    task->broadcast.lastKnownRevision.inSight);

	outputBuffer = putUInt32(outputBuffer, &numberOfPlaques);

	int rowNumber;
	for (rowNumber = 0; rowNumber < numberOfPlaques; rowNumber++)
	{
		char *plaqueToken = PQgetvalue(dbh->result, rowNumber, 0);
		char *plaqueRevision = PQgetvalue(dbh->result, rowNumber, 1);
		char disappeared = *PQgetvalue(dbh->result, rowNumber, 2);
		if ((plaqueToken == NULL) || (plaqueRevision == NULL)) {
			reportError("No results");
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = putData(outputBuffer, plaqueToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, plaqueRevision, sizeof(uint32));
		outputBuffer = putData(outputBuffer, &disappeared, sizeof(disappeared));
	}

	pokeDB(dbh);

	return 0;
}

static int
paquetBroadcastPlaquesOnMap(struct paquet *paquet)
{
	struct task	*task = paquet->task;

    struct buffer *outputBuffer = peekBufferOfSize(task->desk->pools.dynamic, 512, BUFFER_BROADCAST);
	if (outputBuffer == NULL) {
		setTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

    paquet->outputBuffer = outputBuffer;

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 currentRevision;
    if (getSessionOnMapRevision(task, dbh, &currentRevision) != 0) {
		pokeDB(dbh);
		return -1;
	}

    uint32 lastKnownRevision = htobe32(task->broadcast.lastKnownRevision.onMap);
    dbhPushBIGINT(dbh, &task->sessionId);
    dbhPushINTEGER(dbh, &lastKnownRevision);

	dbhExecute(dbh, "\
SELECT plaque_token, plaque_revision, disappeared \
FROM journal.session_on_map_plaques \
JOIN surrounding.plaques \
    USING (plaque_id) \
WHERE session_id = $1 \
  AND on_map_revision > $2");

	if (!dbhTuplesOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!dbhCorrectNumberOfColumns(dbh->result, 3)) {
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

	if (!dbhCorrectColumnType(dbh->result, 2, BOOLOID)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    uint32 broadcastDestination = BroadcastDestinationOnMap;
	outputBuffer = putUInt32(outputBuffer, &broadcastDestination);

	outputBuffer = putUInt32(outputBuffer, &currentRevision);

	uint32 numberOfPlaques = PQntuples(dbh->result);

	reportLog("Found %u plaques for 'on map' current revision %u last known revision %u",
	    numberOfPlaques,
	    currentRevision,
	    task->broadcast.lastKnownRevision.onMap);

	outputBuffer = putUInt32(outputBuffer, &numberOfPlaques);

	int rowNumber;
	for (rowNumber = 0; rowNumber < numberOfPlaques; rowNumber++)
	{
		char *plaqueToken = PQgetvalue(dbh->result, rowNumber, 0);
		char *plaqueRevision = PQgetvalue(dbh->result, rowNumber, 1);
		char disappeared = *PQgetvalue(dbh->result, rowNumber, 2);
		if ((plaqueToken == NULL) || (plaqueRevision == NULL)) {
			reportError("No results");
			pokeDB(dbh);
			setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = putData(outputBuffer, plaqueToken, TokenBinarySize);
		outputBuffer = putData(outputBuffer, plaqueRevision, sizeof(uint32));
		outputBuffer = putData(outputBuffer, &disappeared, sizeof(disappeared));
	}

	pokeDB(dbh);

	return 0;
}
