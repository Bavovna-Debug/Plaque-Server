#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "apns.h"
#include "apns_thread.h"
#include "chalkboard.h"
#include "report.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

void *
APNS_Thread(void *arg);

int
APNS_Start(void)
{
    int rc;

    // For portability, explicitly create threads in a joinable state.
    //
    pthread_attr_init(&chalkboard->apns.attributes);
    pthread_attr_setdetachstate(&chalkboard->apns.attributes, PTHREAD_CREATE_JOINABLE);

    rc = pthread_create(&chalkboard->apns.thread, &chalkboard->apns.attributes, &APNS_Thread, NULL);
    if (rc != 0)
    {
        ReportError("Cannot create APNS thread: %d", errno);
        return -1;
    }

    return 0;
}

int
APNS_Stop(void)
{
    int rc;

    rc = pthread_cancel(chalkboard->apns.thread);
    if ((rc != 0) && (rc != ESRCH))
    {
        ReportError("Cannot cancel APNS thread: rc=%d", rc);
        return -1;
    }

    rc = pthread_attr_destroy(&chalkboard->apns.attributes);
    if (rc != 0)
    {
        ReportError("Cannot destroy thread attributes: rc=%d", rc);
        return -1;
    }

    return 0;
}

void *
APNS_Thread(void *arg)
{
	struct APNS_Connection  *connection;
	struct timespec         ts;
	int                     timedout;
	int                     rc;

    connection = &chalkboard->apns.connection;

    rc = RC_OK;

    while (1)
    {
        // Wait for condition only if this is the first run or if the previous run was successful.
        //
        if (rc == RC_OK)
        {
            // Prepare timer for timed condition wait.
            //
            rc = clock_gettime(CLOCK_REALTIME_COARSE, &ts);
            if (rc == -1)
            {
                ReportError("Cannot get system time: errno=%d", errno);
                rc = RC_ERROR;
                break;
            }

            ts.tv_sec += APNS_DISCONNECT_IF_IDLE;

            // Wait for condition for some time.
            //
            ReportInfo("APNS thread waiting %d seconds for pending notifications",
                APNS_DISCONNECT_IF_IDLE);

            rc = pthread_mutex_lock(&chalkboard->apns.readyToGoMutex);
            if (rc != 0)
            {
                ReportError("Error has occurred on mutex lock: rc=%d", rc);
                rc = RC_ERROR;
                break;
            }

            rc = pthread_cond_timedwait(
                &chalkboard->apns.readyToGoCond,
                &chalkboard->apns.readyToGoMutex,
                &ts);
            timedout = (rc == ETIMEDOUT) ? 1 : 0;
            if ((rc != 0) && (timedout == 0))
            {
                pthread_mutex_unlock(&chalkboard->apns.readyToGoMutex);
                ReportError("Error has occurred while whaiting for condition: rc=%d", rc);
                rc = RC_ERROR;
                break;
            }

            rc = pthread_mutex_unlock(&chalkboard->apns.readyToGoMutex);
            if (rc != 0)
            {
                ReportError("Error has occurred on mutex unlock: rc=%d", rc);
                rc = RC_ERROR;
                break;
            }

            if (timedout == 1)
            {
                // Condition wait has timed out.
                // Disconnect from APNS and start waiting for condition without timer.
                //
                ReportInfo("No pending notifications");

                DisconnectFromAPNS(connection);

                ReportInfo("APNS thread waiting for pending notifications");

                rc = pthread_mutex_lock(&chalkboard->apns.readyToGoMutex);
                if (rc != 0)
                {
                    ReportError("Error has occurred on mutex lock: rc=%d", rc);
                    rc = RC_ERROR;
                    break;
                }

                rc = pthread_cond_wait(
                    &chalkboard->apns.readyToGoCond,
                    &chalkboard->apns.readyToGoMutex);
                if (rc != 0)
                {
                    pthread_mutex_unlock(&chalkboard->apns.readyToGoMutex);
                    ReportError("Error has occurred while whaiting for condition: rc=%d", rc);
                    rc = RC_ERROR;
                    break;
                }

                rc = pthread_mutex_unlock(&chalkboard->apns.readyToGoMutex);
                if (rc != 0)
                {
                    ReportError("Error has occurred on mutex unlock: rc=%d", rc);
                    rc = RC_ERROR;
                    break;
                }
            }
        }

        // Send pending notifications.
        // Retry if some resources are busy.
        //
        do {
            // Establish connection to APNS.
            //
            rc = ConnectToAPNS(connection);
            if (rc != RC_OK) {
                if (rc != RC_ALREADY_CONNECTED)
                {
                    // If any kind of connection error has occurred,
                    // then wait a bit before retrying to connect.
                    //
                    sleep(SLEEP_ON_CONNECT_ERROR);
                    continue;
                }
            }

            rc = SendOneByOne(connection);
            //rc = SendAsFrame(connection);
            if (rc != RC_OK)
            {
                if (rc == RC_RESOURCES_BUSY)
                {
                    //
                    // If some resources are busy,
                    // then wait a bit before retrying to connect.
                    //
                    sleep(SLEEP_ON_BUSY_RESOURCES);
                }
                else if (rc == RC_XMIT_ERROR)
                {
                    //
                    // In case of send/receive error disconnect
                    // and wait a bit before retrying to connect.
                    //
                    DisconnectFromAPNS(connection);

                    sleep(SLEEP_ON_XMIT_ERROR);
                }
                else
                {
                    break;
                }
            }
        } while (rc != RC_OK);

        // In case of any I/O error disconnect and wait a bit.
        //
        if (rc != RC_OK)
        {
            DisconnectFromAPNS(connection);

            sleep(SLEEP_ON_OTHER_ERROR);
        }
    }

	pthread_exit(NULL);
}
