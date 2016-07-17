#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "desk.h"
#include "listener.h"
#include "report.h"
#include "tasks.h"

void *
ListenerCleanup(void *arg);

/**
 * ListenerThread()
 *
 * @arg:
 */
void *
ListenerThread(void *arg)
{
	struct desk         *desk = (struct desk *)arg;
	int                 listenSockFD;
	int                 clientSockFD;
	struct sockaddr_in  serverAddress;
	struct sockaddr_in  clientAddress;
	socklen_t           clientAddressLength;
	const int           socketValue = 1;
	int                 rc;

    pthread_cleanup_push(&ListenerCleanup, desk);

    while (1)
    {
	    listenSockFD = socket(AF_INET, SOCK_STREAM, 0);
	    if (listenSockFD < 0)
	    {
	    	reportError("Cannot open a socket, wait for %d milliseconds: errno=%d",
	    	    SLEEP_ON_CANNOT_OPEN_SOCKET,
	    	    errno);

	    	usleep(SLEEP_ON_CANNOT_OPEN_SOCKET * 1000);

	    	continue;
    	}

        desk->listener.listenSockFD = listenSockFD;

        // Notify the stack that the socket address needs to be reused.
        // Important for the case when the socket needed to be reopened.
        // If it does not work, then close the socket and retry.
        //
        rc = setsockopt(listenSockFD,
            SOL_SOCKET,
            SO_REUSEADDR,
            &socketValue,
            sizeof(socketValue));
        if (rc == -1)
        {
	        reportError("Cannot set socket options, wait for %d milliseconds: errno=%d",
        	    SLEEP_ON_SET_SOCKET_OPTIONS,
        	    errno);

            close(listenSockFD);

            usleep(SLEEP_ON_SET_SOCKET_OPTIONS * 1000);

	        continue;
        }

	    bzero((char *)&serverAddress, sizeof(serverAddress));

	    serverAddress.sin_family = AF_INET;
	    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	    serverAddress.sin_port = htons(desk->listener.portNumber);

        while (1)
        {
        	rc = bind(listenSockFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    	    if (rc == 0) {
    	        //
    	        // Successfully bind to socket.
    	        //
    	        break;
    	    } else {
	            if (errno == EADDRINUSE) {
	                //
	                // Connection refused: wait and try again.
	                //
	    	        reportError("Cannot bind to socket, wait for %d milliseconds",
    	                SLEEP_ON_CANNOT_BIND_SOCKET);

            	    usleep(SLEEP_ON_CANNOT_BIND_SOCKET * 1000);
	            } else {
	                //
	                // Error: close socket, wait, and go to the beginning of socket creation.
	                //
    		        close(listenSockFD);

	    	        reportError("Cannot bind to socket, wait for %d milliseconds: errno=%d",
    	                SLEEP_ON_CANNOT_BIND_SOCKET, errno);

            	    usleep(SLEEP_ON_CANNOT_BIND_SOCKET * 1000);

        	    	continue;
    	        }
	        }
	    }

	    listen(listenSockFD, SOMAXCONN);
	    clientAddressLength = sizeof(clientAddress);
	    while (1)
	    {
		    clientSockFD = accept(listenSockFD,
				(struct sockaddr *)&clientAddress,
				&clientAddressLength);
		    if (clientSockFD < 0)
		    {
			    reportError("Cannot accept new socket, wait for %d milliseconds: errno=%d",
	    	        SLEEP_ON_CANNOT_ACCEPT, errno);

	        	usleep(SLEEP_ON_CANNOT_ACCEPT * 1000);

    	    	break;
		    }

		    char *clientIP = inet_ntoa(clientAddress.sin_addr);

		    struct task *task = startTask(desk, clientSockFD, clientIP);
            if (task == NULL) {
                reportError("Cannot start new task");
			    close(clientSockFD);
                continue;
            }
        }
	}

    pthread_cleanup_pop(1);

	pthread_exit(NULL);
}

/**
 * ListenerCleanup()
 *
 * @arg:
 */
void *
ListenerCleanup(void *arg)
{
	struct desk *desk = (struct desk *)arg;
	int         listenSockFD;

	listenSockFD = desk->listener.listenSockFD;

    if (listenSockFD > 0)
    	close(listenSockFD);
}
