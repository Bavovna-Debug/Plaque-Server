#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "broadcaster_api.h"
#include "broadcaster.h"
#include "desk.h"
#include "report.h"
#include "tasks.h"
#include "task_list.h"

#define BROADCASTER_SLEEP_ON_CANNOT_OPEN_SOCKET    1 * 1000 * 1000  // Microseconds
#define BROADCASTER_SLEEP_ON_CANNOT_CONNECT        2 * 1000 * 1000  // Microseconds
#define TIMEOUT_DISCONNECT_IF_IDLE               300                // Seconds
#define TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT     10 * 1000 * 1000	// Milliseconds
#define TIMEOUT_ON_POLL_FOR_RECEIPT                5 * 1000 * 1000	// Milliseconds

static void
broadcasterDialog(struct desk *desk, int sockFD);

static int
receiveSession(int sockFD, struct session *session);

static int
confirmSession(int sockFD, struct session *session);

void *
broadcasterThread(void *arg)
{
	struct desk *desk = (struct desk *)arg;
	int sockFD;
	struct sockaddr_in broadcasterAddress;
	int rc;

    while (1)
    {
    	sockFD = socket(AF_INET, SOCK_STREAM, 0);
	    if (sockFD < 0) {
	    	reportError("Cannot open a socket, wait for %d microseconds: errno=%d",
	    	    BROADCASTER_SLEEP_ON_CANNOT_OPEN_SOCKET, errno);
	    	usleep(BROADCASTER_SLEEP_ON_CANNOT_OPEN_SOCKET);
    		continue;
	    }

    	bzero((char *)&broadcasterAddress, sizeof(broadcasterAddress));

    	broadcasterAddress.sin_family = AF_INET;
	    broadcasterAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    	broadcasterAddress.sin_port = htons(desk->broadcaster.portNumber);

        while (1)
        {
	        rc = connect(sockFD, (struct sockaddr *)&broadcasterAddress, sizeof(broadcasterAddress));
    	    if (rc == 0) {
    	        //
    	        // Connection established.
    	        //
    	        break;
    	    } else {
	            if (errno == ECONNREFUSED) {
	                //
	                // Connection refused: wait and try again.
	                //
	    	        reportError("Cannot connect to broadcaster, wait for %d microseconds",
    	                BROADCASTER_SLEEP_ON_CANNOT_CONNECT);
            	    usleep(BROADCASTER_SLEEP_ON_CANNOT_CONNECT);
	            } else {
	                //
	                // Error: close socket, wait, and go to the beginning of socket creation.
	                //
    		        close(sockFD);
	    	        reportError("Cannot connect to broadcaster, wait for %d microseconds: errno=%d",
    	                BROADCASTER_SLEEP_ON_CANNOT_CONNECT, errno);
            	    usleep(BROADCASTER_SLEEP_ON_CANNOT_CONNECT);
        	    	continue;
    	        }
	        }
	    }

        broadcasterDialog(desk, sockFD);

	    close(sockFD);
	}

	pthread_exit(NULL);
}

static void
broadcasterDialog(struct desk *desk, int sockFD)
{
	int             sessionNumber;
	struct session  *session;
    uint64          satelliteTaskId;
    struct task     *task;
    int             rc;

    while (1)
    {
        session = &desk->broadcaster.session;

        rc = receiveSession(sockFD, session);
        if (rc != 0)
            break;

        satelliteTaskId = be32toh(session->satelliteTaskId);

        task = taskListTaskById(desk, satelliteTaskId);
        if (task == NULL) {
            reportLog("Task %u is already closed", satelliteTaskId);
        } else {
            //
            // Changing broadcast values has to be done inside of the broadcast lock.
            //
            rc = pthread_mutex_lock(&task->broadcast.editMutex);
            if (rc != 0) {
                reportError("Error has occurred on mutex lock: rc=%d", rc);
                break;
            }

            task->broadcast.currentRevision.onRadar = be32toh(session->onRadarRevision);
            task->broadcast.currentRevision.inSight = be32toh(session->inSightRevision);
            task->broadcast.currentRevision.onMap = be32toh(session->onMapRevision);

            reportLog("Received revised session: receiptId=%lu sessionId=%lu, revisions=%u/%u/%u, taskId=%u",
                be64toh(session->receiptId),
                be64toh(session->sessionId),
                task->broadcast.currentRevision.onRadar,
                task->broadcast.currentRevision.inSight,
                task->broadcast.currentRevision.onMap,
                satelliteTaskId);

            if (task->broadcast.broadcastPaquet != NULL) {
                rc = pthread_mutex_lock(&task->broadcast.waitMutex);
                if (rc != 0) {
                    pthread_mutex_unlock(&task->broadcast.editMutex);
                    reportError("Error has occurred on mutex lock: rc=%d", rc);
                    break;
                }

                rc = pthread_cond_signal(&task->broadcast.waitCondition);
                if (rc != 0) {
                    pthread_mutex_unlock(&task->broadcast.waitMutex);
                    pthread_mutex_unlock(&task->broadcast.editMutex);
                    reportError("Error has occurred on condition signal: rc=%d", rc);
                    break;
                }

                rc = pthread_mutex_unlock(&task->broadcast.waitMutex);
                if (rc != 0) {
                    pthread_mutex_unlock(&task->broadcast.editMutex);
                    reportError("Error has occurred on mutex unlock: rc=%d", rc);
                    break;
                }
            }

            rc = pthread_mutex_unlock(&task->broadcast.editMutex);
            if (rc != 0) {
                reportError("Error has occurred on mutex unlock: rc=%d", rc);
                break;
            }
        }

        rc = confirmSession(sockFD, session);
        if (rc != 0)
            break;
    }
}

static int
receiveSession(int sockFD, struct session *session)
{
	struct pollfd		pollFD;
	ssize_t		        expectedSize;
	ssize_t				receivedPerStep;
	ssize_t				receivedTotal;

	pollFD.fd = sockFD;
	pollFD.events = POLLIN;

	int pollRC = poll(&pollFD, 1, TIMEOUT_ON_POLL_FOR_RECEIPT);
	if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
		reportError("Poll error on receive: revents=0x%04X", pollFD.revents);
		return -1;
	}

	if (pollRC == 0) {
		reportLog("Wait for receive timed out");
		return -1;
	} else if (pollRC != 1) {
		reportError("Poll error on receive");
		return -1;
	}

    expectedSize = sizeof(struct session);
	receivedTotal = 0;

	do {
		receivedPerStep = read(sockFD,
			session + receivedTotal,
			expectedSize - receivedTotal);
		if (receivedPerStep == 0) {
			reportError("Nothing read from socket");
			return -1;
		} else if (receivedPerStep == -1) {
			reportError("Error reading from broadcaster socket: errno=%d", errno);
			return -1;
		}

		receivedTotal += receivedPerStep;
	} while (receivedTotal < expectedSize);

	return 0;
}

static int
confirmSession(int sockFD, struct session *session)
{
	struct pollfd		pollFD;
	int                 pollRC;
	ssize_t				bytesToSend;
	ssize_t				sentPerStep;
	ssize_t				sentTotal;

	pollFD.fd = sockFD;
	pollFD.events = POLLOUT;

	pollRC = poll(&pollFD, 1, TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT);
	if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
		reportError("Poll error on send: revents=0x%04X", pollFD.revents);
		return -1;
	}

	if (pollRC == 0) {
		reportLog("Wait for send timed out");
		return -1;
	} else if (pollRC != 1) {
		reportError("Poll error on send");
		return -1;
	}

	bytesToSend = sizeof(uint64);
	sentTotal = 0;

	do {
		sentPerStep = write(sockFD,
			&session->receiptId + sentTotal,
			bytesToSend - sentTotal);
		if (sentPerStep == 0) {
			reportError("Nothing written to socket");
			return -1;
		} else if (sentPerStep == -1) {
			reportError("Error writing to socket: errno=%d", errno);
			return -1;
		}

		sentTotal += sentPerStep;
	} while (sentTotal < bytesToSend);

    reportLog("Revised session confirmed");

	return 0;
}
