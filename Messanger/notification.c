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
#include <storage/spin.h>
#include <access/xact.h>
#include <executor/spi.h>
#include <fmgr.h>
#include <lib/stringinfo.h>
#include <pgstat.h>
#include <utils/builtins.h>
#include <utils/snapmgr.h>
#include <tcop/utility.h>

#include "buffers.h"
#include "desk.h"
#include "notification.h"
#include "pgbgw.h"
#include "report.h"

static void
deviceTokenCharToBin(char *charToken, char *binToken);

int
resetInMessangerFlag(void)
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
	if (rc != SPI_OK_UPDATE) {
		reportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

    numberOfNotifications = SPI_processed;

   	PGBGW_COMMIT;

	if (numberOfNotifications == 0) {
		reportLog("No notifications were reset");
    } else {
		reportLog("%d notifications were reset", numberOfNotifications);
    }

	return numberOfNotifications;
}

int
numberOfOutstandingNotifications(void)
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
	if (rc != SPI_OK_SELECT) {
		reportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

	if (SPI_processed != 1) {
	    reportError("Unexpected number of tuples");
   	    PGBGW_ROLLBACK;
		return -1;
    }

    tupdesc = SPI_tuptable->tupdesc;
    tuple = SPI_tuptable->vals[0];
    numberOfNotifications = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isNull));

    if (numberOfNotifications > 0)
        reportLog("There are %lu outstanding notifications", numberOfNotifications);

   	PGBGW_COMMIT;

	return numberOfNotifications;
}

int
fetchListOfOutstandingNotifications(struct desk *desk)
{
	StringInfoData      infoData;
    TupleDesc           tupdesc;
	HeapTuple           tuple;
	bool                isNull;
	int                 notificationNumber;
	struct buffer       *buffer;
	struct notification *notification;
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
	if (rc != SPI_OK_SELECT) {
		reportError("Cannot execute statement, rc=%d", rc);
   	    PGBGW_ROLLBACK;
		return -1;
    }

	if (SPI_processed > 0) {
        tupdesc = SPI_tuptable->tupdesc;

        pthread_spin_lock(&desk->outstandingNotifications.lock);

	    for (notificationNumber = 0; notificationNumber < SPI_processed; notificationNumber++)
	    {
	        buffer = peekBuffer(desk->pools.notifications);
	        if (buffer == NULL)
	            break;

            notification = (struct notification *)buffer->data;

        	tuple = SPI_tuptable->vals[notificationNumber];

            notification->notificationId = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isNull));

            desk->outstandingNotifications.buffers = appendBuffer(desk->outstandingNotifications.buffers, buffer);
        }

        pthread_spin_unlock(&desk->outstandingNotifications.lock);
	}

    reportLog("Fetched a list of %u outstanding notifications", SPI_processed);

   	PGBGW_COMMIT;

	return 0;
}

int
fetchNotificationsToMessanger(struct desk *desk)
{
	StringInfoData      infoData;
    TupleDesc           tupdesc;
	HeapTuple           tuple;
	bool                isNull;
	struct buffer       *buffer;
	struct notification *notification;
	char                *deviceToken;
	int                 rc;

    pthread_spin_lock(&desk->outstandingNotifications.lock);

    // If nothing to do, then quit.
    //
    if (desk->outstandingNotifications.buffers == NULL) {
        pthread_spin_unlock(&desk->outstandingNotifications.lock);
        return 0;
    }

	PGBGW_BEGIN;

    buffer = desk->outstandingNotifications.buffers;

    while (buffer != NULL)
    {
        notification = (struct notification *)buffer->data;

    	initStringInfo(&infoData);
	    appendStringInfo(
		    &infoData, "\
SELECT device_id, message_key, '\"'||ARRAY_TO_STRING(message_arguments, '\",\"')||'\"' \
FROM journal.notifications \
WHERE notification_id = %lu",
            notification->notificationId
	    );

        rc = SPI_exec(infoData.data, 1);
	    if (rc != SPI_OK_SELECT) {
            pthread_spin_unlock(&desk->outstandingNotifications.lock);

		    reportError("Cannot execute statement, rc=%d", rc);
   	        PGBGW_ROLLBACK;
		    return -1;
        }

    	if (SPI_processed != 1) {
            pthread_spin_unlock(&desk->outstandingNotifications.lock);

		    reportError("Unexpected number of tuples");
   	        PGBGW_ROLLBACK;
	    	return -1;
        }

        tupdesc = SPI_tuptable->tupdesc;
       	tuple = SPI_tuptable->vals[0];

        notification->deviceId = DatumGetInt64(SPI_getbinval(tuple, tupdesc, 1, &isNull));
        strncpy(notification->messageKey, DatumGetCString(SPI_getvalue(tuple, tupdesc, 2)), MESSAGE_KEY_SIZE);
        strncpy(notification->messageArguments, DatumGetCString(SPI_getvalue(tuple, tupdesc, 3)), MESSAGE_ARGUMENTS_SIZE);

    	initStringInfo(&infoData);
	    appendStringInfo(
		    &infoData, "\
SELECT ENCODE(apns_token, 'hex') \
FROM journal.apns_tokens \
WHERE device_id = %lu",
            notification->deviceId
	    );

        rc = SPI_exec(infoData.data, 1);
	    if (rc != SPI_OK_SELECT) {
            pthread_spin_unlock(&desk->outstandingNotifications.lock);

		    reportError("Cannot execute statement, rc=%d", rc);
   	        PGBGW_ROLLBACK;
		    return -1;
        }

    	if (SPI_processed != 1) {
            pthread_spin_unlock(&desk->outstandingNotifications.lock);

		    reportError("Unexpected number of tuples");
   	        PGBGW_ROLLBACK;
	    	return -1;
        }

        tupdesc = SPI_tuptable->tupdesc;
       	tuple = SPI_tuptable->vals[0];

        deviceToken = DatumGetCString(SPI_getvalue(tuple, tupdesc, 1));
        deviceTokenCharToBin(deviceToken, notification->deviceToken);

        reportLog("Notification %lu for %lu (%s, %s)",
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
	    if (rc != SPI_OK_UPDATE) {
            pthread_spin_unlock(&desk->outstandingNotifications.lock);

		    reportError("Cannot execute statement, rc=%d", rc);
   	        PGBGW_ROLLBACK;
		    return -1;
        }

        buffer = nextBuffer(buffer);
	}

   	PGBGW_COMMIT;

    pthread_spin_unlock(&desk->outstandingNotifications.lock);

	return 0;
}

int
moveOutstandingToInTheAir(struct desk *desk)
{
    pthread_spin_lock(&desk->outstandingNotifications.lock);
    pthread_spin_lock(&desk->inTheAirNotifications.lock);

    desk->inTheAirNotifications.buffers =
        appendBuffer(desk->inTheAirNotifications.buffers, desk->outstandingNotifications.buffers);

    desk->outstandingNotifications.buffers = NULL;

    pthread_spin_unlock(&desk->inTheAirNotifications.lock);
    pthread_spin_unlock(&desk->outstandingNotifications.lock);

    return 0;
}

int
flagSentNotification(struct desk *desk)
{
	StringInfoData      infoData;
	struct buffer       *buffer;
	struct notification *notification;
	int                 rc;

    if (pthread_spin_trylock(&desk->sentNotifications.lock) != 0)
        return 0;

    // If nothing to do, then quit.
    //
    if (desk->sentNotifications.buffers == NULL) {
        pthread_spin_unlock(&desk->sentNotifications.lock);
        return 0;
    }

	PGBGW_BEGIN;

    buffer = desk->sentNotifications.buffers;

    while (buffer != NULL)
    {
        notification = (struct notification *)buffer->data;

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
	    if (rc != SPI_OK_UPDATE) {
            pthread_spin_unlock(&desk->sentNotifications.lock);

		    reportError("Cannot execute statement, rc=%d", rc);
       	    PGBGW_ROLLBACK;
    		return -1;
    	}

        buffer = nextBuffer(buffer);
    }

   	PGBGW_COMMIT;

    pthread_spin_lock(&desk->processedNotifications.lock);
    desk->processedNotifications.buffers =
        appendBuffer(desk->processedNotifications.buffers, desk->sentNotifications.buffers);
    pthread_spin_unlock(&desk->processedNotifications.lock);

    desk->sentNotifications.buffers = NULL;

    pthread_spin_unlock(&desk->sentNotifications.lock);

	return 0;
}

int
releaseProcessedNotificationsFromMessanger(struct desk *desk)
{
	StringInfoData      infoData;
	struct buffer       *buffer;
	struct notification *notification;
	int                 rc;

    pthread_spin_lock(&desk->processedNotifications.lock);

    // If nothing to do, then quit.
    //
    if (desk->processedNotifications.buffers == NULL) {
        pthread_spin_unlock(&desk->processedNotifications.lock);
        return 0;
    }

	PGBGW_BEGIN;

    buffer = desk->processedNotifications.buffers;

    while (buffer != NULL)
    {
        notification = (struct notification *)buffer->data;

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
	    if (rc != SPI_OK_UPDATE) {
            pthread_spin_unlock(&desk->processedNotifications.lock);

		    reportError("Cannot execute statement, rc=%d", rc);
       	    PGBGW_ROLLBACK;
    		return -1;
    	}

        buffer = nextBuffer(buffer);
    }

   	PGBGW_COMMIT;

    pokeBuffer(desk->processedNotifications.buffers);
    desk->processedNotifications.buffers = NULL;

    pthread_spin_unlock(&desk->processedNotifications.lock);

	return 0;
}

static void
deviceTokenCharToBin(char *charToken, char *binToken)
{
    int position;
	int scannedValue;
    char byte[3];
    byte[2] = 0;

    for (position = 0; position < DEVICE_TOKEN_SIZE; position++)
    {
    	strncpy((char *)&byte, (char *)&charToken[position * 2], 2 * sizeof(char));
        sscanf(byte, "%x", &scannedValue);
    	binToken[position] = scannedValue;
    }
}
