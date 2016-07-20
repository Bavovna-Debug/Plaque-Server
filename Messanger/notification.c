#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <postgres.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/lwlock.h>
#include <storage/proc.h>
#include <storage/shmem.h>
#include <access/xact.h>
#include <executor/spi.h>
#include <fmgr.h>
#include <lib/stringinfo.h>
#include <pgstat.h>
#include <utils/builtins.h>
#include <utils/snapmgr.h>
#include <tcop/utility.h>

#include "chalkboard.h"
#include "mmps.h"
#include "notification.h"
#include "pgbgw.h"
#include "report.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

static void
DeviceTokenCharToBin(char *charToken, char *binToken);

int
ResetInMessangerFlag(void)
{
	StringInfoData  infoData;
	uint32          numberOfNotifications;
	int             rc;

	PGBGW_BEGIN;

	initStringInfo(&infoData);
	appendStringInfo(
		&infoData, "\
UPDATE journal.notifications \
SET in_messanger = FALSE \
WHERE in_messanger IS TRUE"
	);

    SetCurrentStatementStartTimestamp();
    rc = SPI_exec(infoData.data, 0);
	if (rc != SPI_OK_UPDATE)
	{
		ReportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

    numberOfNotifications = SPI_processed;

   	PGBGW_COMMIT;

	if (numberOfNotifications == 0) {
		ReportInfo("No notifications were reset");
    } else {
		ReportInfo("%d notifications were reset", numberOfNotifications);
    }

	return numberOfNotifications;
}

int
NumberOfOutstandingNotifications(void)
{
	StringInfoData  infoData;
    TupleDesc       tupdesc;
	HeapTuple       tuple;
	bool            isNull;
	uint64          numberOfNotifications;
	int             rc;

	PGBGW_BEGIN;

	initStringInfo(&infoData);
	appendStringInfo(
		&infoData, "\
SELECT COUNT(*) \
FROM journal.notifications \
WHERE in_messanger IS FALSE \
  AND sent IS FALSE"
	);

    rc = SPI_exec(infoData.data, 1);
	if (rc != SPI_OK_SELECT)
	{
		ReportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

	if (SPI_processed != 1)
	{
	    ReportError("Unexpected number of tuples");
   	    PGBGW_ROLLBACK;
		return -1;
    }

    tupdesc = SPI_tuptable->tupdesc;
    tuple = SPI_tuptable->vals[0];
    numberOfNotifications = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isNull));

    if (numberOfNotifications > 0)
        ReportInfo("There are %lu outstanding notifications", numberOfNotifications);

   	PGBGW_COMMIT;

	return numberOfNotifications;
}

int
FetchListOfOutstandingNotifications(void)
{
	StringInfoData      infoData;
    TupleDesc           tupdesc;
	HeapTuple           tuple;
	bool                isNull;
	int                 notificationNumber;
	struct MMPS_Buffer  *buffer;
	struct Notification *notification;
	int                 rc;

	PGBGW_BEGIN;

	initStringInfo(&infoData);
	appendStringInfo(
		&infoData, "\
SELECT notification_id \
FROM journal.notifications \
WHERE in_messanger IS FALSE \
  AND sent IS FALSE \
ORDER BY notification_id"
	);

    rc = SPI_exec(infoData.data, MAX_NOTIFICATIONS);
	if (rc != SPI_OK_SELECT)
	{
		ReportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

	if (SPI_processed > 0)
	{
        tupdesc = SPI_tuptable->tupdesc;

        pthread_mutex_lock(&chalkboard->outstandingNotifications.mutex);

	    for (notificationNumber = 0; notificationNumber < SPI_processed; notificationNumber++)
	    {
	        buffer = MMPS_PeekBuffer(chalkboard->pools.notifications, BUFFER_NOTIFICATION);
	        if (buffer == NULL)
	            break;

            notification = (struct Notification *) buffer->data;

        	tuple = SPI_tuptable->vals[notificationNumber];

            notification->notificationId = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isNull));

            chalkboard->outstandingNotifications.buffers =
                MMPS_AppendBuffer(chalkboard->outstandingNotifications.buffers, buffer);
        }

        pthread_mutex_unlock(&chalkboard->outstandingNotifications.mutex);
	}

    ReportInfo("Fetched a list of %u outstanding notifications", SPI_processed);

   	PGBGW_COMMIT;

	return 0;
}

int
FetchNotificationsToMessanger(void)
{
	StringInfoData      infoData;
    TupleDesc           tupdesc;
	HeapTuple           tuple;
	bool                isNull;
	struct MMPS_Buffer  *buffer;
	struct Notification *notification;
	char                *deviceToken;
	int                 rc;

    pthread_mutex_lock(&chalkboard->outstandingNotifications.mutex);

    // If nothing to do, then quit.
    //
    if (chalkboard->outstandingNotifications.buffers == NULL)
    {
        pthread_mutex_unlock(&chalkboard->outstandingNotifications.mutex);
        return 0;
    }

	PGBGW_BEGIN;

    buffer = chalkboard->outstandingNotifications.buffers;

    while (buffer != NULL)
    {
        notification = (struct Notification *) buffer->data;

    	initStringInfo(&infoData);
	    appendStringInfo(
		    &infoData, "\
SELECT device_id, message_key, '\"'||ARRAY_TO_STRING(message_arguments, '\",\"')||'\"' \
FROM journal.notifications \
WHERE notification_id = %lu",
            notification->notificationId
	    );

        rc = SPI_exec(infoData.data, 1);
	    if (rc != SPI_OK_SELECT)
	    {
            pthread_mutex_unlock(&chalkboard->outstandingNotifications.mutex);

		    ReportError("Cannot execute statement, rc=%d", rc);
   	        PGBGW_ROLLBACK;
		    return -1;
        }

    	if (SPI_processed != 1)
    	{
            pthread_mutex_unlock(&chalkboard->outstandingNotifications.mutex);

		    ReportError("Unexpected number of tuples");
   	        PGBGW_ROLLBACK;
	    	return -1;
        }

        tupdesc = SPI_tuptable->tupdesc;
       	tuple = SPI_tuptable->vals[0];

        notification->deviceId = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isNull));

        strncpy(
            notification->messageKey,
            DatumGetCString(SPI_getvalue(tuple, tupdesc, 2)),
            MESSAGE_KEY_SIZE);

        strncpy(
            notification->messageArguments,
            DatumGetCString(SPI_getvalue(tuple, tupdesc, 3)),
            MESSAGE_ARGUMENTS_SIZE);

    	initStringInfo(&infoData);
	    appendStringInfo(
		    &infoData, "\
SELECT ENCODE(apns_token, 'hex') \
FROM journal.apns_tokens \
WHERE device_id = %lu",
            notification->deviceId
	    );

        rc = SPI_exec(infoData.data, 1);
	    if (rc != SPI_OK_SELECT)
	    {
            pthread_mutex_unlock(&chalkboard->outstandingNotifications.mutex);

		    ReportError("Cannot execute statement, rc=%d", rc);
   	        PGBGW_ROLLBACK;
		    return -1;
        }

    	if (SPI_processed != 1)
    	{
            pthread_mutex_unlock(&chalkboard->outstandingNotifications.mutex);

		    ReportError("Unexpected number of tuples");
   	        PGBGW_ROLLBACK;
	    	return -1;
        }

        tupdesc = SPI_tuptable->tupdesc;
       	tuple = SPI_tuptable->vals[0];

        deviceToken = DatumGetCString(SPI_getvalue(tuple, tupdesc, 1));
        DeviceTokenCharToBin(deviceToken, notification->deviceToken);

        ReportInfo("Notification %lu for %lu (%s, %s)",
            notification->notificationId,
            notification->deviceId,
            notification->messageKey,
            notification->messageArguments);

    	initStringInfo(&infoData);
	    appendStringInfo(
		    &infoData, "\
UPDATE journal.notifications \
SET in_messanger = TRUE \
WHERE notification_id = %lu",
            notification->notificationId
	    );

        SetCurrentStatementStartTimestamp();
        rc = SPI_exec(infoData.data, 0);
	    if (rc != SPI_OK_UPDATE)
	    {
            pthread_mutex_unlock(&chalkboard->outstandingNotifications.mutex);

		    ReportError("Cannot execute statement, rc=%d", rc);
   	        PGBGW_ROLLBACK;
		    return -1;
        }

        buffer = MMPS_NextBuffer(buffer);
	}

   	PGBGW_COMMIT;

    pthread_mutex_unlock(&chalkboard->outstandingNotifications.mutex);

	return 0;
}

int
MoveOutstandingToInTheAir(void)
{
    pthread_mutex_lock(&chalkboard->outstandingNotifications.mutex);
    pthread_mutex_lock(&chalkboard->inTheAirNotifications.mutex);

    chalkboard->inTheAirNotifications.buffers =
        MMPS_AppendBuffer(
            chalkboard->inTheAirNotifications.buffers,
            chalkboard->outstandingNotifications.buffers);

    chalkboard->outstandingNotifications.buffers = NULL;

    pthread_mutex_unlock(&chalkboard->inTheAirNotifications.mutex);
    pthread_mutex_unlock(&chalkboard->outstandingNotifications.mutex);

    return 0;
}

int
FlagSentNotification(void)
{
	StringInfoData      infoData;
	struct MMPS_Buffer  *buffer;
	struct Notification *notification;
	int                 rc;

    if (pthread_mutex_trylock(&chalkboard->sentNotifications.mutex) != 0)
        return 0;

    // If nothing to do, then quit.
    //
    if (chalkboard->sentNotifications.buffers == NULL)
    {
        pthread_mutex_unlock(&chalkboard->sentNotifications.mutex);
        return 0;
    }

	PGBGW_BEGIN;

    buffer = chalkboard->sentNotifications.buffers;

    while (buffer != NULL)
    {
        notification = (struct Notification *) buffer->data;

    	initStringInfo(&infoData);
	    appendStringInfo(
		    &infoData, "\
UPDATE journal.notifications \
SET sent = TRUE \
WHERE notification_id = %lu",
            notification->notificationId
	    );

        SetCurrentStatementStartTimestamp();
        rc = SPI_exec(infoData.data, 0);
	    if (rc != SPI_OK_UPDATE)
	    {
            pthread_mutex_unlock(&chalkboard->sentNotifications.mutex);

		    ReportError("Cannot execute statement, rc=%d", rc);
       	    PGBGW_ROLLBACK;
    		return -1;
    	}

        buffer = MMPS_NextBuffer(buffer);
    }

   	PGBGW_COMMIT;

    pthread_mutex_lock(&chalkboard->processedNotifications.mutex);
    chalkboard->processedNotifications.buffers =
        MMPS_AppendBuffer(
            chalkboard->processedNotifications.buffers,
            chalkboard->sentNotifications.buffers);
    pthread_mutex_unlock(&chalkboard->processedNotifications.mutex);

    chalkboard->sentNotifications.buffers = NULL;

    pthread_mutex_unlock(&chalkboard->sentNotifications.mutex);

	return 0;
}

int
ReleaseProcessedNotificationsFromMessanger(void)
{
	StringInfoData      infoData;
	struct MMPS_Buffer  *buffer;
	struct Notification *notification;
	int                 rc;

    pthread_mutex_lock(&chalkboard->processedNotifications.mutex);

    // If nothing to do, then quit.
    //
    if (chalkboard->processedNotifications.buffers == NULL)
    {
        pthread_mutex_unlock(&chalkboard->processedNotifications.mutex);
        return 0;
    }

	PGBGW_BEGIN;

    buffer = chalkboard->processedNotifications.buffers;

    while (buffer != NULL)
    {
        notification = (struct Notification *) buffer->data;

    	initStringInfo(&infoData);
	    appendStringInfo(
		    &infoData, "\
UPDATE journal.notifications \
SET in_messanger = FALSE \
WHERE notification_id = %lu",
            notification->notificationId
	    );

        SetCurrentStatementStartTimestamp();
        rc = SPI_exec(infoData.data, 0);
	    if (rc != SPI_OK_UPDATE)
	    {
            pthread_mutex_unlock(&chalkboard->processedNotifications.mutex);

		    ReportError("Cannot execute statement, rc=%d", rc);
       	    PGBGW_ROLLBACK;
    		return -1;
    	}

        buffer = MMPS_NextBuffer(buffer);
    }

   	PGBGW_COMMIT;

    MMPS_PokeBuffer(chalkboard->processedNotifications.buffers);
    chalkboard->processedNotifications.buffers = NULL;

    pthread_mutex_unlock(&chalkboard->processedNotifications.mutex);

	return 0;
}

static void
DeviceTokenCharToBin(char *charToken, char *binToken)
{
    int position;
	int scannedValue;
    char byte[3];
    byte[2] = 0;

    for (position = 0; position < DEVICE_TOKEN_SIZE; position++)
    {
    	strncpy((char *) &byte, (char *) &charToken[position * 2], 2 * sizeof(char));
        sscanf(byte, "%x", &scannedValue);
    	binToken[position] = scannedValue;
    }
}
