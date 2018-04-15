#include <c.h>
#include <string.h>

#include "api.h"
#include "chalkboard.h"
#include "db.h"
#include "mmps.h"
#include "paquet.h"
#include "plaques_query.h"
#include "report.h"
#include "tasks.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

#define QUERY_SELECT_PLAQUES "\
SELECT plaque_revision, profile_token, time_slot_id, dimension, latitude, longitude, altitude, \
    direction, tilt, width, height, \
    background_color, foreground_color, font_size, inscription \
FROM surrounding.plaques \
JOIN auth.profiles USING (profile_id) \
WHERE plaque_token = $1"

int
HandleDownloadPlaques(struct Paquet *paquet)
{
    struct Task *task = paquet->task;

    struct MMPS_Buffer *inputBuffer = paquet->inputBuffer;
    struct MMPS_Buffer *outputBuffer = NULL;

    uint32 numberOfPlaques;

    if (!MinimumPayloadSize(paquet, sizeof(numberOfPlaques))) {
        SetTaskStatus(task, TaskStatusWrongPayloadSize);
        return -1;
    }

    MMPS_ResetCursor(inputBuffer);

    inputBuffer = MMPS_GetInt32(inputBuffer, &numberOfPlaques);

    if (!ExpectedPayloadSize(paquet, sizeof(numberOfPlaques) + numberOfPlaques * API_TokenBinarySize))
    {
        ReportError("Wrong payload for %d plaques", numberOfPlaques);
        SetTaskStatus(task, TaskStatusWrongPayloadSize);
        return -1;
    }

    outputBuffer = MMPS_PeekBufferOfSize(chalkboard->pools.dynamic, KB, BUFFER_PLAQUES);
    if (outputBuffer == NULL) {
        SetTaskStatus(task, TaskStatusCannotAllocateBufferForOutput);
        return -1;
    }

    paquet->outputBuffer = outputBuffer;

    MMPS_ResetBufferData(outputBuffer);

    struct dbh *dbh = DB_PeekHandle(chalkboard->db.plaque);
    if (dbh == NULL) {
        SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
        return -1;
    }

    char plaqueToken[API_TokenBinarySize];
    int i;
    for (i = 0; i < numberOfPlaques; i++)
    {
        inputBuffer = MMPS_GetData(inputBuffer, plaqueToken, API_TokenBinarySize, NULL);

        DB_PushUUID(dbh, plaqueToken);

        DB_Execute(dbh, QUERY_SELECT_PLAQUES);

        if (!DB_TuplesOK(dbh, dbh->result)) {
            DB_PokeHandle(dbh);
            SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
            return -1;
        }

        if (!DB_CorrectNumberOfColumns(dbh->result, 15)) {
            DB_PokeHandle(dbh);
            SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
            return -1;
        }

        if (!DB_CorrectNumberOfRows(dbh->result, 1)) {
            ReportInfo("Requested plaque not found");
            continue;
//          DB_PokeHandle(dbh);
//          SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
//          return -1;
        }

        if (!DB_CorrectColumnType(dbh->result, 0, INT4OID) ||
            !DB_CorrectColumnType(dbh->result, 1, UUIDOID) ||
            !DB_CorrectColumnType(dbh->result, 2, INT8OID) ||
            //!DB_CorrectColumnType(dbh->result, 3, CHAROID) ||
            !DB_CorrectColumnType(dbh->result, 4, FLOAT8OID) ||
            !DB_CorrectColumnType(dbh->result, 5, FLOAT8OID) ||
            !DB_CorrectColumnType(dbh->result, 6, FLOAT4OID) ||
            !DB_CorrectColumnType(dbh->result, 7, FLOAT4OID) ||
            !DB_CorrectColumnType(dbh->result, 8, FLOAT4OID) ||
            !DB_CorrectColumnType(dbh->result, 9, FLOAT4OID) ||
            !DB_CorrectColumnType(dbh->result, 10, FLOAT4OID) ||
            !DB_CorrectColumnType(dbh->result, 11, INT4OID) ||
            !DB_CorrectColumnType(dbh->result, 12, INT4OID) ||
            !DB_CorrectColumnType(dbh->result, 13, FLOAT4OID) ||
            !DB_CorrectColumnType(dbh->result, 14, TEXTOID))
        {
            DB_PokeHandle(dbh);
            SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
            return -1;
        }

        char *plaqueRevision    = PQgetvalue(dbh->result, 0, 0);
        char *profileToken      = PQgetvalue(dbh->result, 0, 1);
        char *timeSlotId        = PQgetvalue(dbh->result, 0, 2);
        uint8 fortified          = PQgetisnull(dbh->result, 0, 2) ? 0 : 1;
        char *dimension         = PQgetvalue(dbh->result, 0, 3);
        char *latitude          = PQgetvalue(dbh->result, 0, 4);
        char *longitude         = PQgetvalue(dbh->result, 0, 5);
        char *altitude          = PQgetvalue(dbh->result, 0, 6);
        uint8 directed          = PQgetisnull(dbh->result, 0, 7) ? 0 : 1;
        char *direction         = PQgetvalue(dbh->result, 0, 7);
        uint8 tilted            = PQgetisnull(dbh->result, 0, 8) ? 0 : 1;
        char *tilt              = PQgetvalue(dbh->result, 0, 8);
        char *width             = PQgetvalue(dbh->result, 0, 9);
        char *height            = PQgetvalue(dbh->result, 0, 10);
        char *backgroundColor   = PQgetvalue(dbh->result, 0, 11);
        char *foregroundColor   = PQgetvalue(dbh->result, 0, 12);
        char *fontSize          = PQgetvalue(dbh->result, 0, 13);
        char *inscription       = PQgetvalue(dbh->result, 0, 14);
        int inscriptionSize     = PQgetlength(dbh->result, 0, 14);

        if ((plaqueRevision == NULL) || (profileToken == NULL) || (timeSlotId == NULL)
            || (dimension == NULL) || (latitude == NULL) || (longitude == NULL) || (altitude == NULL)
            || (direction == NULL) || (tilt == NULL) || (width == NULL) || (height == NULL)
            || (backgroundColor == NULL) || (foregroundColor == NULL) || (fontSize == NULL)
            || (inscription == NULL)) {
            ReportError("No results");
            DB_PokeHandle(dbh);
            SetTaskStatus(task, TaskStatusUnexpectedDatabaseResult);
            return -1;
        }

        uint32 plaqueStrobe = API_PaquetPlaqueStrobe;
        outputBuffer = MMPS_PutInt32(outputBuffer, &plaqueStrobe);

        outputBuffer = MMPS_PutData(outputBuffer, plaqueToken, API_TokenBinarySize);
        outputBuffer = MMPS_PutData(outputBuffer, plaqueRevision, sizeof(uint32));
        outputBuffer = MMPS_PutData(outputBuffer, profileToken, API_TokenBinarySize);
        outputBuffer = MMPS_PutInt8(outputBuffer, &fortified);
        outputBuffer = MMPS_PutData(outputBuffer, dimension, 2 * sizeof(char));
        outputBuffer = MMPS_PutData(outputBuffer, latitude, sizeof(double));
        outputBuffer = MMPS_PutData(outputBuffer, longitude, sizeof(double));
        outputBuffer = MMPS_PutData(outputBuffer, altitude, sizeof(float));
        outputBuffer = MMPS_PutInt8(outputBuffer, &directed);
        outputBuffer = MMPS_PutData(outputBuffer, direction, sizeof(float));
        outputBuffer = MMPS_PutInt8(outputBuffer, &tilted);
        outputBuffer = MMPS_PutData(outputBuffer, tilt, sizeof(float));
        outputBuffer = MMPS_PutData(outputBuffer, width, sizeof(float));
        outputBuffer = MMPS_PutData(outputBuffer, height, sizeof(float));
        outputBuffer = MMPS_PutData(outputBuffer, backgroundColor, sizeof(uint32));
        outputBuffer = MMPS_PutData(outputBuffer, foregroundColor, sizeof(uint32));
        outputBuffer = MMPS_PutData(outputBuffer, fontSize, sizeof(float));
        outputBuffer = MMPS_PutString(outputBuffer, inscription, inscriptionSize);
    }

    DB_PokeHandle(dbh);

    return 0;
}
