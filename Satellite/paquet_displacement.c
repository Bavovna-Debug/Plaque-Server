#include <c.h>
#include <string.h>

#include "db.h"
#include "buffers.h"
#include "paquet.h"
#include "paquet_displacement.h"
#include "report.h"
#include "tasks.h"

static void
journalUserLocation(
    struct task *task,
    struct dbh *dbh,
    struct paquetDisplacement *displacement);

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

	struct paquetDisplacement *displacement = (struct paquetDisplacement *)inputBuffer->cursor;

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    journalUserLocation(task, dbh, displacement);

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

	struct paquetDisplacement *displacement = (struct paquetDisplacement *)inputBuffer->cursor;

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    journalUserLocation(task, dbh, displacement);

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

int
paquetDisplacementOnMap(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	struct buffer *inputBuffer = paquet->inputBuffer;
	struct buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetDisplacement))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	resetCursor(inputBuffer, 1);

	struct paquetDisplacement *displacement = (struct paquetDisplacement *)inputBuffer->cursor;

	struct dbh *dbh = peekDB(task->desk->dbh.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    journalUserLocation(task, dbh, displacement);

    dbhPushBIGINT(dbh, &task->deviceId);
    dbhPushDOUBLE(dbh, &displacement->latitude);
    dbhPushDOUBLE(dbh, &displacement->longitude);
    dbhPushREAL(dbh, &displacement->range);

	dbhExecute(dbh, "\
UPDATE journal.device_displacements \
SET on_map_coordinate = LL_TO_EARTH($2, $3), \
    on_map_range = $4 \
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

static void
journalUserLocation(
    struct task *task,
    struct dbh *dbh,
    struct paquetDisplacement *displacement)
{
    dbhPushBIGINT(dbh, &task->deviceId);
    dbhPushDOUBLE(dbh, &displacement->latitude);
    dbhPushDOUBLE(dbh, &displacement->longitude);
    dbhPushREAL(dbh, &displacement->altitude);

	if (displacement->courseAvailable == DeviceBooleanFalse) {
        dbhPushREAL(dbh, NULL);
	} else {
        dbhPushREAL(dbh, &displacement->course);
	}

	if (displacement->floorLevelAvailable == DeviceBooleanFalse) {
        dbhPushINTEGER(dbh, NULL);
	} else {
        dbhPushINTEGER(dbh, &displacement->floorLevel);
	}

	dbhExecute(dbh, "\
INSERT INTO journal.movements \
(device_id, latitude, longitude, altitude, course, floor_level) \
VALUES ($1, $2, $3, $4, $5, $6)");
}
