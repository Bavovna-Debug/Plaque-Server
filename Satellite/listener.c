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

	    bzero((char *)&serverAddress, sizeof(serverAddress));

	    serverAddress.sin_family = AF_INET;
	    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	    serverAddress.sin_port = htons(desk->listener.portNumber);

	    if (bind(listenSockFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
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

		    char *clientIP = inet_ntoa(clientAddress.sin_addr);

		    struct task *task = startTask(desk, clientSockFD, clientIP);
            if (task == NULL) {
                reportError("Cannot start new task");
			    close(clientSockFD);
                continue;
            }
        }
	}

	close(listenSockFD);

	pthread_exit(NULL);
}
