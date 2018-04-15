#include <c.h>
#include <string.h>

#include "db.h"
#include "chalkboard.h"
#include "mmps.h"
#include "paquet.h"
#include "paquet_displacement.h"
#include "report.h"
#include "tasks.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

static void
JournalUserLocation(
    struct Task *task,
    struct dbh *dbh,
    struct PaquetDisplacement *displacement);

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
HandleDisplacementOnRadar(struct Paquet *paquet)
{
	struct Task	*task = paquet->task;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!ExpectedPayloadSize(paquet, sizeof(struct PaquetDisplacement))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetDisplacement *displacement = (struct PaquetDisplacement *) inputBuffer->cursor;

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    JournalUserLocation(task, dbh, displacement);

    DB_PushBIGINT(dbh, &task->deviceId);
    DB_PushDOUBLE(dbh, &displacement->latitude);
    DB_PushDOUBLE(dbh, &displacement->longitude);
    DB_PushREAL(dbh, &displacement->range);

	DB_Execute(dbh, QUERY_DISPLACEMENT_ON_RADAR);

	if (!DB_CommandOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

    uint32 result = API_PaquetDisplacementSucceeded;
    outputBuffer = MMPS_PutInt32(outputBuffer, &result);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
HandleDisplacementInSight(struct Paquet *paquet)
{
	struct Task	*task = paquet->task;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!ExpectedPayloadSize(paquet, sizeof(struct PaquetDisplacement))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetDisplacement *displacement = (struct PaquetDisplacement *) inputBuffer->cursor;

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    JournalUserLocation(task, dbh, displacement);

    DB_PushBIGINT(dbh, &task->deviceId);
    DB_PushDOUBLE(dbh, &displacement->latitude);
    DB_PushDOUBLE(dbh, &displacement->longitude);
    DB_PushREAL(dbh, &displacement->range);

	DB_Execute(dbh, QUERY_DISPLACEMENT_IN_SIGHT);

	if (!DB_CommandOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

    uint32 result = API_PaquetDisplacementSucceeded;
    outputBuffer = MMPS_PutInt32(outputBuffer, &result);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

int
HandleDisplacementOnMap(struct Paquet *paquet)
{
	struct Task	*task = paquet->task;

	struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
	struct MMPS_Buffer *outputBuffer = paquet->inputBuffer;

	if (!ExpectedPayloadSize(paquet, sizeof(struct PaquetDisplacement))) {
		SetTaskStatus(task, TaskStatusWrongPayloadSize);
		return -1;
	}

	MMPS_ResetCursor(inputBuffer);

	struct PaquetDisplacement *displacement = (struct PaquetDisplacement *) inputBuffer->cursor;

	struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
	if (dbh == NULL) {
		SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

    JournalUserLocation(task, dbh, displacement);

    DB_PushBIGINT(dbh, &task->deviceId);
    DB_PushDOUBLE(dbh, &displacement->latitude);
    DB_PushDOUBLE(dbh, &displacement->longitude);
    DB_PushREAL(dbh, &displacement->range);

	DB_Execute(dbh, QUERY_DISPLACEMENT_ON_MAP);

	if (!DB_CommandOK(dbh, dbh->result)) {
		DB_PokeHandle(dbh);
		SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
		return -1;
	}

	MMPS_ResetBufferData(outputBuffer);

    uint32 result = API_PaquetDisplacementSucceeded;
    outputBuffer = MMPS_PutInt32(outputBuffer, &result);

	DB_PokeHandle(dbh);

	paquet->outputBuffer = paquet->inputBuffer;

	return 0;
}

static void
JournalUserLocation(
    struct Task *task,
    struct dbh *dbh,
    struct PaquetDisplacement *displacement)
{
    DB_PushBIGINT(dbh, &task->deviceId);
    DB_PushDOUBLE(dbh, &displacement->latitude);
    DB_PushDOUBLE(dbh, &displacement->longitude);
    DB_PushREAL(dbh, &displacement->altitude);

	if (displacement->courseAvailable == API_DeviceBooleanFalse) {
        DB_PushREAL(dbh, NULL);
	} else {
        DB_PushREAL(dbh, &displacement->course);
	}

	if (displacement->floorLevelAvailable == API_DeviceBooleanFalse) {
        DB_PushINTEGER(dbh, NULL);
	} else {
        DB_PushINTEGER(dbh, &displacement->floorLevel);
	}

	DB_Execute(dbh, QUERY_REGISTER_MOVEMENT);
}
