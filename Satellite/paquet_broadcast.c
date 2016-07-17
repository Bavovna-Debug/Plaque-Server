#include <c.h>
#include <string.h>

#include "api.h"
#include "chalkboard.h"
#include "db.h"
#include "mmps.h"
#include "paquet.h"
#include "paquet_broadcast.h"
#include "report.h"
#include "session.h"
#include "tasks.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

static int
HandleBroadcastPlaquesOnRadar(struct Paquet *paquet);

static int
HandleBroadcastPlaquesInSight(struct Paquet *paquet);

static int
HandleBroadcastPlaquesOnMap(struct Paquet *paquet);

int
HandleBroadcast(struct Paquet *paquet)
{
    struct Task         *task;
    struct Revisions    *lastKnownRevision;
    struct Revisions    *currentRevision;
    struct Revisions    missingRevision;
    int                 rc;

    task = paquet->task;

    lastKnownRevision = &task->broadcast.lastKnownRevision;
    currentRevision = &task->broadcast.currentRevision;

	if (!ExpectedPayloadSize(paquet, sizeof(struct PaquetBroadcast))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(paquet->inputBuffer, 1);

	struct PaquetBroadcast *broadcast = (struct PaquetBroadcast *) paquet->inputBuffer->cursor;

	lastKnownRevision->onRadar = be32toh(broadcast->lastKnownOnRadarRevision);
	lastKnownRevision->inSight = be32toh(broadcast->lastKnownInSightRevision);
	lastKnownRevision->onMap = be32toh(broadcast->lastKnownOnMapRevision);

    rc = GetSessionRevisions(task, currentRevision);
    if (rc != 0) {
		SetTaskStatus(task, TaskStatusOtherError);
		return -1;
    }

    ReportInfo("'On radar' revisions: 'last known' %u 'current' %u",
        lastKnownRevision->onRadar,
        currentRevision->onRadar);
    ReportInfo("'In sight' revisions: 'last known' %u 'current' %u",
        lastKnownRevision->inSight,
        currentRevision->inSight);
    ReportInfo("'On map' revisions: 'last known' %u 'current' %u",
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
		ReportInfo("Broadcast request received while another broadcast request is still in process");
	    return -1;
	}

	task->broadcast.broadcastPaquet = paquet;

	pthread_mutex_unlock(&task->broadcast.editMutex);

    missingRevision.onRadar = currentRevision->onRadar - lastKnownRevision->onRadar;
    missingRevision.inSight = currentRevision->inSight - lastKnownRevision->inSight;
    missingRevision.onMap = currentRevision->onMap - lastKnownRevision->onMap;

    if ((missingRevision.onRadar > 0) || (missingRevision.inSight > 0) || (missingRevision.onMap > 0)) {
        ReportInfo("Do not wait for boradcast because there are already %u / %u / %u missing revisions",
            missingRevision.onRadar,
            missingRevision.inSight,
            missingRevision.onMap);
    } else {
        ReportInfo("Waiting for broadcast with known revisions %u / %u / %u",
            lastKnownRevision->onRadar,
            lastKnownRevision->inSight,
            lastKnownRevision->onMap);

        rc = pthread_mutex_lock(&task->broadcast.waitMutex);
        if (rc != 0) {
            ReportError("Error has occurred on mutex lock: rc=%d", rc);
            return -1;
        }

        rc = pthread_cond_wait(&task->broadcast.waitCondition, &task->broadcast.waitMutex);
        if (rc != 0) {
            pthread_mutex_unlock(&task->broadcast.waitMutex);
            ReportError("Error has occurred while whaiting for condition: rc=%d", rc);
            return -1;
        }

        rc = pthread_mutex_unlock(&task->broadcast.waitMutex);
        if (rc != 0) {
            ReportError("Error has occurred on mutex unlock: rc=%d", rc);
            return -1;
        }

        ReportInfo("Received broadcast");
    }

	pthread_mutex_lock(&task->broadcast.editMutex);

    if (currentRevision->onRadar > lastKnownRevision->onRadar) {
        ReportInfo("Fetch 'on radar' for broadcast from revision %u to %u",
            lastKnownRevision->onRadar,
            currentRevision->onRadar);
        rc = HandleBroadcastPlaquesOnRadar(paquet);
    } else if (currentRevision->inSight > lastKnownRevision->inSight) {
        ReportInfo("Fetch 'in sight' for broadcast from revision %u to %u",
            lastKnownRevision->inSight,
            currentRevision->inSight);
        rc = HandleBroadcastPlaquesInSight(paquet);
    } else if (currentRevision->onMap > lastKnownRevision->onMap) {
        ReportInfo("Fetch 'on map' for broadcast from revision %u to %u",
            lastKnownRevision->onMap,
            currentRevision->onMap);
        rc = HandleBroadcastPlaquesOnMap(paquet);
    } else {
        ReportInfo("Nothing to fetch for broadcast");
        rc = -1;
    }

	task->broadcast.broadcastPaquet = NULL;

	pthread_mutex_unlock(&task->broadcast.editMutex);

    return rc;
}

#define QUERY_SELECT_PLAQUES_ON_RADAR "\
SELECT plaque_token, plaque_revision, disappeared \
FROM journal.session_on_radar_plaques \
JOIN surrounding.plaques \
    USING (plaque_id) \
WHERE session_id = $1 \
  AND on_radar_revision > $2"

#define QUERY_SELECT_PLAQUES_IN_SIGHT "\
SELECT plaque_token, plaque_revision, disappeared \
FROM journal.session_in_sight_plaques \
JOIN surrounding.plaques \
    USING (plaque_id) \
WHERE session_id = $1 \
  AND in_sight_revision > $2"

#define QUERY_SELECT_PLAQUES_ON_MAP "\
SELECT plaque_token, plaque_revision, disappeared \
FROM journal.session_on_map_plaques \
JOIN surrounding.plaques \
    USING (plaque_id) \
WHERE session_id = $1 \
  AND on_map_revision > $2"

static int
HandleBroadcastPlaquesOnRadar(struct Paquet *paquet)
{
	struct Task	*task = paquet->task;

    struct MMPS_Buffer *outputBuffer =
        MMPS_PeekBufferOfSize(chalkboard->pools.dynamic, 512, BUFFER_BROADCAST);
	if (outputBuffer == NULL) {
		SetTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

    paquet->outputBuffer = outputBuffer;

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 currentRevision;
    if (GetSessionOnRadarRevision(task, dbh, &currentRevision) != 0) {
		DB_PokeHandle(dbh);
		return -1;
	}

    uint32 lastKnownRevision = htobe32(task->broadcast.lastKnownRevision.onRadar);
    DB_PushBIGINT(dbh, &task->sessionId);
    DB_PushINTEGER(dbh, &lastKnownRevision);

	DB_Execute(dbh, QUERY_SELECT_PLAQUES_ON_RADAR);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 3)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 1, INT4OID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 2, BOOLOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

    uint32 broadcastDestination = API_BroadcastDestinationOnRadar;
	outputBuffer = MMPS_PutInt32(outputBuffer, &broadcastDestination);

	outputBuffer = MMPS_PutInt32(outputBuffer, &currentRevision);

	uint32 numberOfPlaques = PQntuples(dbh->result);

	ReportInfo("Found %u plaques for 'on radar' current revision %u last known revision %u",
	    numberOfPlaques,
	    currentRevision,
	    task->broadcast.lastKnownRevision.onRadar);

	outputBuffer = MMPS_PutInt32(outputBuffer, &numberOfPlaques);

	int rowNumber;
	for (rowNumber = 0; rowNumber < numberOfPlaques; rowNumber++)
	{
		char *plaqueToken = PQgetvalue(dbh->result, rowNumber, 0);
		char *plaqueRevision = PQgetvalue(dbh->result, rowNumber, 1);
		char disappeared = *PQgetvalue(dbh->result, rowNumber, 2);
		if ((plaqueToken == NULL) || (plaqueRevision == NULL)) {
			ReportError("No results");
			DB_PokeHandle(dbh);
			SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = MMPS_PutData(outputBuffer, plaqueToken, API_TokenBinarySize);
		outputBuffer = MMPS_PutData(outputBuffer, plaqueRevision, sizeof(uint32));
		outputBuffer = MMPS_PutData(outputBuffer, &disappeared, sizeof(disappeared));
	}

	DB_PokeHandle(dbh);

	return 0;
}

static int
HandleBroadcastPlaquesInSight(struct Paquet *paquet)
{
	struct Task	*task = paquet->task;

    struct MMPS_Buffer *outputBuffer =
        MMPS_PeekBufferOfSize(chalkboard->pools.dynamic, 512, BUFFER_BROADCAST);
	if (outputBuffer == NULL) {
		SetTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

    paquet->outputBuffer = outputBuffer;

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 currentRevision;
    if (GetSessionInSightRevision(task, dbh, &currentRevision) != 0) {
		DB_PokeHandle(dbh);
		return -1;
	}

    uint32 lastKnownRevision = htobe32(task->broadcast.lastKnownRevision.inSight);
    DB_PushBIGINT(dbh, &task->sessionId);
    DB_PushINTEGER(dbh, &lastKnownRevision);

	DB_Execute(dbh, QUERY_SELECT_PLAQUES_IN_SIGHT);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 3)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 1, INT4OID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 2, BOOLOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

    uint32 broadcastDestination = API_BroadcastDestinationInSight;
	outputBuffer = MMPS_PutInt32(outputBuffer, &broadcastDestination);

	outputBuffer = MMPS_PutInt32(outputBuffer, &currentRevision);

	uint32 numberOfPlaques = PQntuples(dbh->result);

	ReportInfo("Found %u plaques for 'in sight' current revision %u last known revision %u",
	    numberOfPlaques,
	    currentRevision,
	    task->broadcast.lastKnownRevision.inSight);

	outputBuffer = MMPS_PutInt32(outputBuffer, &numberOfPlaques);

	int rowNumber;
	for (rowNumber = 0; rowNumber < numberOfPlaques; rowNumber++)
	{
		char *plaqueToken = PQgetvalue(dbh->result, rowNumber, 0);
		char *plaqueRevision = PQgetvalue(dbh->result, rowNumber, 1);
		char disappeared = *PQgetvalue(dbh->result, rowNumber, 2);
		if ((plaqueToken == NULL) || (plaqueRevision == NULL)) {
			ReportError("No results");
			DB_PokeHandle(dbh);
			SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = MMPS_PutData(outputBuffer, plaqueToken, API_TokenBinarySize);
		outputBuffer = MMPS_PutData(outputBuffer, plaqueRevision, sizeof(uint32));
		outputBuffer = MMPS_PutData(outputBuffer, &disappeared, sizeof(disappeared));
	}

	DB_PokeHandle(dbh);

	return 0;
}

static int
HandleBroadcastPlaquesOnMap(struct Paquet *paquet)
{
	struct Task	*task = paquet->task;

    struct MMPS_Buffer *outputBuffer =
        MMPS_PeekBufferOfSize(chalkboard->pools.dynamic, 512, BUFFER_BROADCAST);
	if (outputBuffer == NULL) {
		SetTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

    paquet->outputBuffer = outputBuffer;

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    uint32 currentRevision;
    if (GetSessionOnMapRevision(task, dbh, &currentRevision) != 0) {
		DB_PokeHandle(dbh);
		return -1;
	}

    uint32 lastKnownRevision = htobe32(task->broadcast.lastKnownRevision.onMap);
    DB_PushBIGINT(dbh, &task->sessionId);
    DB_PushINTEGER(dbh, &lastKnownRevision);

	DB_Execute(dbh, QUERY_SELECT_PLAQUES_ON_MAP);

	if (!DB_TuplesOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectNumberOfColumns(dbh->result, 3)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 0, UUIDOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 1, INT4OID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	if (!DB_CorrectColumnType(dbh->result, 2, BOOLOID)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

    uint32 broadcastDestination = API_BroadcastDestinationOnMap;
	outputBuffer = MMPS_PutInt32(outputBuffer, &broadcastDestination);

	outputBuffer = MMPS_PutInt32(outputBuffer, &currentRevision);

	uint32 numberOfPlaques = PQntuples(dbh->result);

	ReportInfo("Found %u plaques for 'on map' current revision %u last known revision %u",
	    numberOfPlaques,
	    currentRevision,
	    task->broadcast.lastKnownRevision.onMap);

	outputBuffer = MMPS_PutInt32(outputBuffer, &numberOfPlaques);

	int rowNumber;
	for (rowNumber = 0; rowNumber < numberOfPlaques; rowNumber++)
	{
		char *plaqueToken = PQgetvalue(dbh->result, rowNumber, 0);
		char *plaqueRevision = PQgetvalue(dbh->result, rowNumber, 1);
		char disappeared = *PQgetvalue(dbh->result, rowNumber, 2);
		if ((plaqueToken == NULL) || (plaqueRevision == NULL)) {
			ReportError("No results");
			DB_PokeHandle(dbh);
			SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
			return -1;
		}

		outputBuffer = MMPS_PutData(outputBuffer, plaqueToken, API_TokenBinarySize);
		outputBuffer = MMPS_PutData(outputBuffer, plaqueRevision, sizeof(uint32));
		outputBuffer = MMPS_PutData(outputBuffer, &disappeared, sizeof(disappeared));
	}

	DB_PokeHandle(dbh);

	return 0;
}
