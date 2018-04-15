#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "chalkboard.h"
#include "listener.h"
#include "report.h"
#include "tasks.h"

/**
 * Take a pointer to chalkboard. Chalkboard must be initialized
 * before any routine of this module could be called.
 */
extern struct Chalkboard *chalkboard;

void
IPv4ListenerCleanup(void *arg);

void
IPv6ListenerCleanup(void *arg);

/**
 * IPv4ListenerThread()
 *
 * @arg:
 */
void *
IPv4ListenerThread(void *arg)
{
	int                 listenSockFD;
	int                 clientSockFD;
	struct sockaddr_in  serverAddress;
	struct sockaddr_in  clientAddress;
	socklen_t           clientAddressLength;
	const int           socketValue = 1;
	char                clientIP[INET_ADDRSTRLEN];
	struct Task         *task;
	int                 rc;

    pthread_cleanup_push(&IPv4ListenerCleanup, NULL);

    for (;;)
    {
	    listenSockFD = socket(AF_INET, SOCK_STREAM, 0);
	    if (listenSockFD < 0)
	    {
	    	ReportError("Cannot open a socket, wait for %d milliseconds: errno=%d",
	    	    SLEEP_ON_CANNOT_OPEN_SOCKET,
	    	    errno);

	    	usleep(SLEEP_ON_CANNOT_OPEN_SOCKET * 1000);

	    	continue;
    	}

        chalkboard->listenerIPv4.listenSockFD = listenSockFD;

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
	        ReportError("Cannot set socket options, wait for %d milliseconds: errno=%d",
        	    SLEEP_ON_SET_SOCKET_OPTIONS,
        	    errno);

            close(listenSockFD);

            usleep(SLEEP_ON_SET_SOCKET_OPTIONS * 1000);

	        continue;
        }

	    bzero((char *) &serverAddress, sizeof(serverAddress));

	    serverAddress.sin_family = AF_INET;
	    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	    serverAddress.sin_port = htons(chalkboard->listenerIPv4.portNumber);

        for (;;)
        {
        	rc = bind(listenSockFD,
        	    (struct sockaddr *) &serverAddress,
        	    sizeof(serverAddress));
    	    if (rc == 0)
            {
    	        // Successfully bind to socket.
    	        //
    	        break;
    	    }
            else
            {
	            if (errno == EADDRINUSE)
                {
	                // Connection refused: wait and try again.
	                //
	    	        ReportError("Cannot bind to socket, wait for %d milliseconds",
    	                SLEEP_ON_CANNOT_BIND_SOCKET);

            	    usleep(SLEEP_ON_CANNOT_BIND_SOCKET * 1000);
	            }
                else
                {
	                // Error: close socket, wait, and go to the beginning of socket creation.
	                //
    		        close(listenSockFD);

	    	        ReportError("Cannot bind to socket, wait for %d milliseconds: errno=%d",
    	                SLEEP_ON_CANNOT_BIND_SOCKET, errno);

            	    usleep(SLEEP_ON_CANNOT_BIND_SOCKET * 1000);

        	    	continue;
    	        }
	        }
	    }

	    listen(listenSockFD, SOMAXCONN);
	    clientAddressLength = sizeof(clientAddress);
	    for (;;)
	    {
		    clientSockFD = accept(listenSockFD,
				(struct sockaddr *) &clientAddress,
				&clientAddressLength);
		    if (clientSockFD < 0)
		    {
			    ReportError("Cannot accept new socket, wait for %d milliseconds: errno=%d",
	    	        SLEEP_ON_CANNOT_ACCEPT, errno);

	        	usleep(SLEEP_ON_CANNOT_ACCEPT * 1000);

    	    	break;
		    }

            inet_ntop(AF_INET, (char *) &clientAddress.sin_addr, (char *) &clientIP, sizeof(clientIP));

		    task = StartTask(clientSockFD, clientIP);
            if (task == NULL)
            {
                ReportError("Cannot start new task");
			    close(clientSockFD);
                continue;
            }
        }
	}

    pthread_cleanup_pop(1);

	pthread_exit(NULL);
}

/**
 * IPv4ListenerCleanup()
 *
 * @arg:
 */
void
IPv4ListenerCleanup(void *arg)
{
	int listenSockFD;

	listenSockFD = chalkboard->listenerIPv4.listenSockFD;

    if (listenSockFD > 0)
    	close(listenSockFD);
}

/**
 * IPv6ListenerThread()
 *
 * @arg:
 */
void *
IPv6ListenerThread(void *arg)
{
    int                 listenSockFD;
    int                 clientSockFD;
    struct sockaddr_in6 serverAddress;
    struct sockaddr_in6 clientAddress;
    socklen_t           clientAddressLength;
    const int           socketValue = 1;
    char                clientIP[INET6_ADDRSTRLEN];
    struct Task         *task;
    int                 rc;

    pthread_cleanup_push(&IPv6ListenerCleanup, NULL);

    for (;;)
    {
        listenSockFD = socket(AF_INET6, SOCK_STREAM, 0);
        if (listenSockFD < 0)
        {
            ReportError("Cannot open a socket, wait for %d milliseconds: errno=%d",
                SLEEP_ON_CANNOT_OPEN_SOCKET,
                errno);

            usleep(SLEEP_ON_CANNOT_OPEN_SOCKET * 1000);

            continue;
        }

        chalkboard->listenerIPv6.listenSockFD = listenSockFD;

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
            ReportError("Cannot set socket options, wait for %d milliseconds: errno=%d",
                SLEEP_ON_SET_SOCKET_OPTIONS,
                errno);

            close(listenSockFD);

            usleep(SLEEP_ON_SET_SOCKET_OPTIONS * 1000);

            continue;
        }

        bzero((char *) &serverAddress, sizeof(serverAddress));

        serverAddress.sin6_family = AF_INET6;
        //serverAddress.sin6_addr = IN6ADDR_ANY_INIT;
        serverAddress.sin6_port = htons(chalkboard->listenerIPv6.portNumber);

        for (;;)
        {
            rc = bind(listenSockFD,
                (struct sockaddr *) &serverAddress,
                sizeof(serverAddress));
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
                    ReportError("Cannot bind to socket, wait for %d milliseconds",
                        SLEEP_ON_CANNOT_BIND_SOCKET);

                    usleep(SLEEP_ON_CANNOT_BIND_SOCKET * 1000);
                } else {
                    //
                    // Error: close socket, wait, and go to the beginning of socket creation.
                    //
                    close(listenSockFD);

                    ReportError("Cannot bind to socket, wait for %d milliseconds: errno=%d",
                        SLEEP_ON_CANNOT_BIND_SOCKET, errno);

                    usleep(SLEEP_ON_CANNOT_BIND_SOCKET * 1000);

                    continue;
                }
            }
        }

        listen(listenSockFD, SOMAXCONN);
        clientAddressLength = sizeof(clientAddress);
        for (;;)
        {
            clientSockFD = accept(listenSockFD,
                (struct sockaddr *) &clientAddress,
                &clientAddressLength);
            if (clientSockFD < 0)
            {
                ReportError("Cannot accept new socket, wait for %d milliseconds: errno=%d",
                    SLEEP_ON_CANNOT_ACCEPT, errno);

                usleep(SLEEP_ON_CANNOT_ACCEPT * 1000);

                break;
            }

            inet_ntop(AF_INET6, (char *) &clientAddress.sin6_addr, (char *) &clientIP, sizeof(clientIP));

            task = StartTask(clientSockFD, clientIP);
            if (task == NULL) {
                ReportError("Cannot start new task");
                close(clientSockFD);
                continue;
            }
        }
    }

    pthread_cleanup_pop(1);

    pthread_exit(NULL);
}

/**
 * IPv6ListenerCleanup()
 *
 * @arg:
 */
void
IPv6ListenerCleanup(void *arg)
{
    int listenSockFD;

    listenSockFD = chalkboard->listenerIPv6.listenSockFD;

    if (listenSockFD > 0)
        close(listenSockFD);
}
