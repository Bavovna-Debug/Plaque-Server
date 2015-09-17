#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <postgres.h>

#include "broadcaster_api.h"
#include "desk.h"
#include "listener.h"
#include "report.h"

static void
conversation(struct desk *desk, int sockFD);

static void
xmit(struct desk *desk, int sockFD);

static int
sendReceipt(int sockFD, struct session *session);

static int
receiveReceipt(int sockFD, struct session *session);

void *
listenerThread(void *arg)
{
	struct desk *desk = (struct desk *)arg;

	int listenSockFD, clientSockFD;
	struct sockaddr_in serverAddress, clientAddress;
	socklen_t clientAddressLength;

    while (1)
    {
	    listenSockFD = socket(AF_INET, SOCK_STREAM, 0);
    	if (listenSockFD < 0) {
	    	reportError("Cannot open a socket, wait for %d microseconds: errno=%d\n",
	    	    SLEEP_ON_CANNOT_OPEN_SOCKET, errno);
	    	usleep(SLEEP_ON_CANNOT_OPEN_SOCKET);
	    	continue;
    	}

	    bzero((char *) &serverAddress, sizeof(serverAddress));

    	serverAddress.sin_family = AF_INET;
	    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    	serverAddress.sin_port = htons(desk->listener.portNumber);

	    if (bind(listenSockFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
		    close(listenSockFD);
    		reportError("Cannot bind to socket, wait for %d microseconds: errno=%d\n",
	    	    SLEEP_ON_CANNOT_BIND_SOCKET, errno);
	    	usleep(SLEEP_ON_CANNOT_BIND_SOCKET);
	    	continue;
    	}

	    listen(listenSockFD, SOMAXCONN);
    	clientAddressLength = sizeof(clientAddress);
	    while (1)
    	{
	    	clientSockFD = accept(listenSockFD,
				(struct sockaddr *)&clientAddress,
				&clientAddressLength);
		    if (clientSockFD < 0) {
			    reportError("Cannot accept new socket, wait for %d microseconds: errno=%d\n",
	    	        SLEEP_ON_CANNOT_ACCEPT, errno);
	        	usleep(SLEEP_ON_CANNOT_ACCEPT);
    	    	break;
		    }

    		reportLog("Accepted connection from %s:%d",
	    	    inet_ntoa(clientAddress.sin_addr),
		        clientAddress.sin_port);

            conversation(desk, clientSockFD);

	    	close(clientSockFD);
	    }

    	close(listenSockFD);
    }

	pthread_exit(NULL);
}

void
listenerKnockKnock(struct desk *desk)
{
    reportLog("Broadcaster knock... knock...");
    sem_post(desk->listener.readyToGo);
}

static void
conversation(struct desk *desk, int sockFD)
{
	struct timespec     ts;
	int                 rc;

    // Immediately after a connection is established try to send revised sessions
    // in case there are some pending.
    //
    xmit(desk, sockFD);

    while (1)
    {
        // Prepare timer for semaphore.
        //
        rc = clock_gettime(CLOCK_REALTIME_COARSE, &ts);
        if (rc == -1) {
            reportError("Cannot get system time: errno=%d", errno);
            break;
        }

        ts.tv_sec += TIMEOUT_DISCONNECT_IF_IDLE;

        // Wait for semaphore for some time.
        //
        reportLog("Broadcaster thread whating %d seconds for revised sessions",
            TIMEOUT_DISCONNECT_IF_IDLE);
        rc = sem_timedwait(desk->listener.readyToGo, &ts);
        if (rc == -1) {
            if (errno != ETIMEDOUT) {
                //
                // Semaphore error! Break the loop.
                //
                reportError("Error has ocurred while whaiting for timed semaphore: errno=%d", errno);
                break;
            } else {
                //
                // Semaphore has timed out.
                // Disconnect from APNS and start waiting for semaphore without timer.
                //
                reportLog("Broadcaster connection idle for %d seconds - disconnect",
                    TIMEOUT_DISCONNECT_IF_IDLE);

                break;
            }
        } else {
            //
            // Semaphore show green light to start transfer.
            //
            xmit(desk, sockFD);
        }
    }
}

static void
xmit(struct desk *desk, int sockFD)
{
	int                 sessionNumber;
	struct session      *session;
    int                 rc;

    pthread_spin_lock(&desk->watchdog.lock);

    if (desk->watchdog.numberOfSessions > 0)
    {
        rc = 0;
	    for (sessionNumber = 0; sessionNumber < desk->watchdog.numberOfSessions; sessionNumber++)
    	{
        	session = &desk->watchdog.sessions[sessionNumber];

            rc = sendReceipt(sockFD, session);
            if (rc != 0)
                break;

            rc = receiveReceipt(sockFD, session);
        }

        if (rc == 0)
            desk->watchdog.numberOfSessions = 0;
    }

    pthread_spin_unlock(&desk->watchdog.lock);
}

static int
sendReceipt(int sockFD, struct session *session)
{
	struct pollfd		pollFD;
	int                 pollRC;
	ssize_t				bytesToSend;
	ssize_t				sentTotal;
	ssize_t				sentPerStep;

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
		reportError("Poll error on receive");
		return -1;
	}

	bytesToSend = sizeof(struct session);
	sentTotal = 0;
	do {
		sentPerStep = write(sockFD,
				session + sentTotal,
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

	return 0;
}

static int
receiveReceipt(int sockFD, struct session *session)
{
	return 0;
}
