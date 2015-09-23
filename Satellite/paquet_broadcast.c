#include <c.h>
#include <semaphore.h>
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
paquetBroadcastForOnRadar(struct paquet *paquet)
{
    struct task *task;
	uint32      lastKnownRevision;
	uint32      currentRevisions;
	uint32      missingRevisions;
    int         rc;

    task = paquet->task;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetBroadcast))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(paquet->inputBuffer, 1);
	getUInt32(paquet->inputBuffer, &lastKnownRevision);

	pokeBuffer(paquet->inputBuffer);
	paquet->inputBuffer = NULL;

	pthread_spin_lock(&task->paquet.broadcastLock);

	if (task->paquet.broadcastOnRadar != NULL)
		paquetCancel(task->paquet.broadcastOnRadar);

	task->paquet.broadcastOnRadar = paquet;
    task->lastKnownRevision.onRadar = lastKnownRevision;

	pthread_spin_unlock(&task->paquet.broadcastLock);

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    rc = getSessionOnRadarRevision(task, dbh, &currentRevisions);
    if (rc != 0) {
        pokeDB(dbh);
		setTaskStatus(task, TaskStatusOtherError);
		return -1;
    }

    pokeDB(dbh);

    if (lastKnownRevision > currentRevisions)
        lastKnownRevision = 0;

    missingRevisions = currentRevisions - lastKnownRevision;

    if (missingRevisions == 0) {
        reportLog("Waiting for broadcast 'on radar' with known revision %u",
            task->lastKnownRevision.onRadar);

        rc = sem_wait(&task->paquet.waitForBroadcastOnRadar);
        if (rc == -1) {
            //
            // Semaphore error! Break the loop.
            //
            reportError("Error has ocurred while whaiting for semaphore: errno=%d", errno);
            return -1;
        }

        reportLog("Received broadcast 'on radar'");
    } else {
        reportLog("Do not wait for boradcast 'on radar' because there are already %u missing revisions", missingRevisions);
    }

    rc = paquetBroadcastPlaquesOnRadar(paquet);

	pthread_spin_lock(&task->paquet.broadcastLock);

	task->paquet.broadcastOnRadar = NULL;
    task->lastKnownRevision.onRadar = 0;

	pthread_spin_unlock(&task->paquet.broadcastLock);

    return rc;
}

int
paquetBroadcastForInSight(struct paquet *paquet)
{
    struct task *task;
	uint32      lastKnownRevision;
	uint32      currentRevisions;
	uint32      missingRevisions;
    int         rc;

    task = paquet->task;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetBroadcast))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(paquet->inputBuffer, 1);
	getUInt32(paquet->inputBuffer, &lastKnownRevision);

	pokeBuffer(paquet->inputBuffer);
	paquet->inputBuffer = NULL;

	pthread_spin_lock(&task->paquet.broadcastLock);

	if (task->paquet.broadcastInSight != NULL)
		paquetCancel(task->paquet.broadcastInSight);

    task->paquet.broadcastInSight = paquet;
    task->lastKnownRevision.inSight = lastKnownRevision;

    pthread_spin_unlock(&task->paquet.broadcastLock);

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    rc = getSessionInSightRevision(task, dbh, &currentRevisions);
    if (rc != 0) {
        pokeDB(dbh);
		setTaskStatus(task, TaskStatusOtherError);
		return -1;
    }

    pokeDB(dbh);

    if (lastKnownRevision > currentRevisions)
        lastKnownRevision = 0;

    missingRevisions = currentRevisions - lastKnownRevision;

    if (missingRevisions == 0) {
        reportLog("Waiting for broadcast 'in sight' with known revision %u",
            task->lastKnownRevision.inSight);

        rc = sem_wait(&task->paquet.waitForBroadcastInSight);
        if (rc == -1) {
            //
            // Semaphore error! Break the loop.
            //
            reportError("Error has ocurred while whaiting for semaphore: errno=%d", errno);
            return -1;
        }

        reportLog("Received broadcast 'in sight'");
    } else {
        reportLog("Do not wait for boradcast 'in sight' because there are already %u missing revisions", missingRevisions);
    }

    rc = paquetBroadcastPlaquesInSight(paquet);

	pthread_spin_lock(&task->paquet.broadcastLock);

	task->paquet.broadcastInSight = NULL;
    task->lastKnownRevision.inSight = 0;

	pthread_spin_unlock(&task->paquet.broadcastLock);

    return rc;
}

int
paquetBroadcastForOnMap(struct paquet *paquet)
{
    struct task *task;
	uint32      lastKnownRevision;
	uint32      currentRevisions;
	uint32      missingRevisions;
    int         rc;

    task = paquet->task;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetBroadcast))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(paquet->inputBuffer, 1);
	getUInt32(paquet->inputBuffer, &lastKnownRevision);

	pokeBuffer(paquet->inputBuffer);
	paquet->inputBuffer = NULL;

	pthread_spin_lock(&task->paquet.broadcastLock);

	if (task->paquet.broadcastOnMap != NULL)
		paquetCancel(task->paquet.broadcastOnMap);

    task->paquet.broadcastOnMap = paquet;
    task->lastKnownRevision.onMap = lastKnownRevision;

    pthread_spin_unlock(&task->paquet.broadcastLock);

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    rc = getSessionOnMapRevision(task, dbh, &currentRevisions);
    if (rc != 0) {
        pokeDB(dbh);
		setTaskStatus(task, TaskStatusOtherError);
		return -1;
    }

    pokeDB(dbh);

    if (lastKnownRevision > currentRevisions)
        lastKnownRevision = 0;

    missingRevisions = currentRevisions - lastKnownRevision;

    if (missingRevisions == 0) {
        reportLog("Waiting for broadcast 'on map' with known revision %u",
            task->lastKnownRevision.onMap);

        rc = sem_wait(&task->paquet.waitForBroadcastOnMap);
        if (rc == -1) {
            //
            // Semaphore error! Break the loop.
            //
            reportError("Error has ocurred while whaiting for semaphore: errno=%d", errno);
            return -1;
        }

        reportLog("Received broadcast 'on map'");
    } else {
        reportLog("Do not wait for boradcast 'on map' because there are already %u missing revisions", missingRevisions);
    }

    rc = paquetBroadcastPlaquesOnMap(paquet);

	pthread_spin_lock(&task->paquet.broadcastLock);

	task->paquet.broadcastOnMap = NULL;
    task->lastKnownRevision.onMap = 0;

	pthread_spin_unlock(&task->paquet.broadcastLock);

    return rc;
}

int
paquetDisplacementOnRadar(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetDisplacement))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	struct paquetDisplacement *displacement = (struct paquetDisplacement *)inputBuffer->cursor;

    dbhPushBIGINT(dbh, &task->deviceId);
    dbhPushDOUBLE(dbh, &displacement->latitude);
    dbhPushDOUBLE(dbh, &displacement->longitude);
    dbhPushREAL(dbh, &displacement->range);

	dbhExecute(dbh, "\
UPDATE journal.device_displacements \
SET on_radar_coordinate = LL_TO_EARTH($2, $3), \
    on_radar_range = $4 \
WHERE device_id = $1");

	if (!dbhCommandOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

    uint32 result = PaquetCreatePlaqueSucceeded;
    outputBuffer = putUInt32(outputBuffer, &result);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
paquetDisplacementInSight(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetDisplacement))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	struct paquetDisplacement *displacement = (struct paquetDisplacement *)inputBuffer->cursor;

    dbhPushBIGINT(dbh, &task->deviceId);
    dbhPushDOUBLE(dbh, &displacement->latitude);
    dbhPushDOUBLE(dbh, &displacement->longitude);
    dbhPushREAL(dbh, &displacement->range);

	dbhExecute(dbh, "\
UPDATE journal.device_displacements \
SET in_sight_coordinate = LL_TO_EARTH($2, $3), \
    in_sight_range = $4 \
WHERE device_id = $1");

	if (!dbhCommandOK(dbh, dbh->result)) {
		pokeDB(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	resetBufferData(outputBuffer, 1);

    uint32 result = PaquetCreatePlaqueSucceeded;
    outputBuffer = putUInt32(outputBuffer, &result);

	pokeDB(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

static int
paquetBroadcastPlaquesOnRadar(struct paquet *paquet)
{
	struct task	*task = paquet->task;

    struct buffer *outputBuffer = peekBufferOfSize(task->desk->pools.dynamic, 256);

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

    uint32 lastKnownRevision = htobe32(task->lastKnownRevision.onRadar);
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

	outputBuffer = putUInt32(outputBuffer, &currentRevision);

	uint32 numberOfPlaques = PQntuples(dbh->result);

	reportLog("Found %u plaques for 'on radar' current revision %u last known revision %u",
	    numberOfPlaques,
	    currentRevision,
	    task->lastKnownRevision.onRadar);

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

    paquet->outputBuffer = outputBuffer;

	return 0;
}

static int
paquetBroadcastPlaquesInSight(struct paquet *paquet)
{
	struct task	*task = paquet->task;

    struct buffer *outputBuffer = peekBufferOfSize(task->desk->pools.dynamic, 256);

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

    uint32 lastKnownRevision = htobe32(task->lastKnownRevision.inSight);
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

	outputBuffer = putUInt32(outputBuffer, &currentRevision);

	uint32 numberOfPlaques = PQntuples(dbh->result);

	reportLog("Found %u plaques for 'in sight' current revision %u last known revision %u",
	    numberOfPlaques,
	    currentRevision,
	    task->lastKnownRevision.inSight);

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

    paquet->outputBuffer = outputBuffer;

	return 0;
}

static int
paquetBroadcastPlaquesOnMap(struct paquet *paquet)
{
	struct task	*task = paquet->task;

    struct buffer *outputBuffer = peekBufferOfSize(task->desk->pools.dynamic, 256);

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

    uint32 lastKnownRevision = htobe32(task->lastKnownRevision.onMap);
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

	resetBufferData(outputBuffer, 1);

	outputBuffer = putUInt32(outputBuffer, &currentRevision);

	uint32 numberOfPlaques = PQntuples(dbh->result);

	reportLog("Found %u plaques for 'on map' current revision %u last known revision %u",
	    numberOfPlaques,
	    currentRevision,
	    task->lastKnownRevision.onMap);

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

    paquet->outputBuffer = outputBuffer;

	return 0;
}
