#include <c.h>
#include <string.h>

#include "db.h"
#include "mmps.h"
#include "paquet.h"
#include "paquet_displacement.h"
#include "report.h"
#include "tasks.h"

static void
journalUserLocation(
    struct task *task,
    struct dbh *dbh,
    struct paquetDisplacement *displacement);

#define QUERY_DISPLACEMENT_ON_RADAR "\
UPDATE journal.device_displacements \
SET on_radar_coordinate = LL_TO_EARTH($2, $3), \
    on_radar_range = $4 \
WHERE device_id = $1"

#define QUERY_DISPLACEMENT_IN_SIGHT "\
UPDATE journal.device_displacements \
SET in_sight_coordinate = LL_TO_EARTH($2, $3), \
    in_sight_range = $4 \
WHERE device_id = $1"

#define QUERY_DISPLACEMENT_ON_MAP "\
UPDATE journal.device_displacements \
SET on_map_coordinate = LL_TO_EARTH($2, $3), \
    on_map_range = $4 \
WHERE device_id = $1"

#define QUERY_REGISTER_MOVEMENT "\
INSERT INTO journal.movements \
(device_id, latitude, longitude, altitude, course, floor_level) \
VALUES ($1, $2, $3, $4, $5, $6)"

int
paquetDisplacementOnRadar(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetDisplacement))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer, 1);

	struct paquetDisplacement *displacement = (struct paquetDisplacement *)inputBuffer->cursor;

	struct dbh *dbh = DB_PeekHandle(task->desk->db.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    journalUserLocation(task, dbh, displacement);

    DB_PushBIGINT(dbh, &task->deviceId);
    DB_PushDOUBLE(dbh, &displacement->latitude);
    DB_PushDOUBLE(dbh, &displacement->longitude);
    DB_PushREAL(dbh, &displacement->range);

	DB_Execute(dbh, QUERY_DISPLACEMENT_ON_RADAR);

	if (!DB_CommandOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

    uint32 result = PaquetCreatePlaqueSucceeded;
    outputBuffer = MMPS_PutInt32(outputBuffer, &result);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
paquetDisplacementInSight(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetDisplacement))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer, 1);

	struct paquetDisplacement *displacement = (struct paquetDisplacement *)inputBuffer->cursor;

	struct dbh *dbh = DB_PeekHandle(task->desk->db.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    journalUserLocation(task, dbh, displacement);

    DB_PushBIGINT(dbh, &task->deviceId);
    DB_PushDOUBLE(dbh, &displacement->latitude);
    DB_PushDOUBLE(dbh, &displacement->longitude);
    DB_PushREAL(dbh, &displacement->range);

	DB_Execute(dbh, QUERY_DISPLACEMENT_IN_SIGHT);

	if (!DB_CommandOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

    uint32 result = PaquetCreatePlaqueSucceeded;
    outputBuffer = MMPS_PutInt32(outputBuffer, &result);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
paquetDisplacementOnMap(struct paquet *paquet)
{
	struct task	*task = paquet->task;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!expectedPayloadSize(paquet, sizeof(struct paquetDisplacement))) {
		setTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer, 1);

	struct paquetDisplacement *displacement = (struct paquetDisplacement *)inputBuffer->cursor;

	struct dbh *dbh = DB_PeekHandle(task->desk->db.plaque);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    journalUserLocation(task, dbh, displacement);

    DB_PushBIGINT(dbh, &task->deviceId);
    DB_PushDOUBLE(dbh, &displacement->latitude);
    DB_PushDOUBLE(dbh, &displacement->longitude);
    DB_PushREAL(dbh, &displacement->range);

	DB_Execute(dbh, QUERY_DISPLACEMENT_ON_MAP);

	if (!DB_CommandOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		setTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer, 1);

    uint32 result = PaquetCreatePlaqueSucceeded;
    outputBuffer = MMPS_PutInt32(outputBuffer, &result);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

static void
journalUserLocation(
    struct task *task,
    struct dbh *dbh,
    struct paquetDisplacement *displacement)
{
    DB_PushBIGINT(dbh, &task->deviceId);
    DB_PushDOUBLE(dbh, &displacement->latitude);
    DB_PushDOUBLE(dbh, &displacement->longitude);
    DB_PushREAL(dbh, &displacement->altitude);

	if (displacement->courseAvailable == DeviceBooleanFalse) {
        DB_PushREAL(dbh, NULL);
	} else {
        DB_PushREAL(dbh, &displacement->course);
	}

	if (displacement->floorLevelAvailable == DeviceBooleanFalse) {
        DB_PushINTEGER(dbh, NULL);
	} else {
        DB_PushINTEGER(dbh, &displacement->floorLevel);
	}

	DB_Execute(dbh, QUERY_REGISTER_MOVEMENT);
}
