#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "buffers.h"
#include "db.h"
#include "queue.h"

void *statisticsThread(void *arg)
{
	while (1)
	{
		printf("############    DBH:%4d\tTASK:%4d\tRCVR:%4d\tXMIT:%4d\t1K:%5d\t4K:%5d\t1M:%5d\n",
			dbhInUse(),
			tasksInUse(),
			receiversInUse(),
			transmittersInUse(),
			buffersInUse(BUFFER1K),
			buffersInUse(BUFFER4K),
			buffersInUse(BUFFER1M));
		sleep(1);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	if (PQisthreadsafe() != 1)
		exit(-1);

	uint16_t portNumber = atoi(argv[1]);

	pthread_attr_t attr;
	int rc;
	rc = pthread_attr_init(&attr);
	if (rc != 0)
		fprintf(stderr, "pthread_attr_init: %d\n", rc);

	int stackSize = 0x800000;
	rc = pthread_attr_setstacksize(&attr, stackSize);
	if (rc != 0)
		fprintf(stderr, "pthread_attr_setstacksize: %d\n", rc);

	constructQueue();

	constructDB();

	constructBuffers();

	pthread_t statisticsHandler;
	pthread_create(&statisticsHandler, NULL, &statisticsThread, NULL);

	int listenSockFD, clientSockFD;
	struct sockaddr_in serverAddress, clientAddress;
	socklen_t clientAddressLength;

	listenSockFD = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSockFD < 0)
		perror("Cannot open a socket");

	bzero((char *) &serverAddress, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(portNumber);
	if (bind(listenSockFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
		close(listenSockFD);
		perror("Cannot bind to socket");
	}

	listen(listenSockFD, SOMAXCONN);
	clientAddressLength = sizeof(clientAddress);
	while (1)
	{
		clientSockFD = accept(listenSockFD, (struct sockaddr *) &clientAddress, &clientAddressLength);
		if (clientSockFD < 0) {
			fprintf(stderr, "Cannot accept new socket: %d\n", errno);
			sleep(1.0);
			continue;
		}

		char *clientIP = strdup(inet_ntoa(clientAddress.sin_addr));

		struct task *task = peekTask(clientSockFD, clientIP);
        if (task == NULL) {
            fprintf(stderr, "All tasks are busy\n");
			close(clientSockFD);
			free(clientIP);
			//sleep(0.2);
            continue;
        }
	}

	close(listenSockFD);

	destructQueue();

	destructDB();

	destructBuffers();

	return 0;
}
