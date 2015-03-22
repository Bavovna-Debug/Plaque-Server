#include <errno.h>
#include <libpq-fe.h>
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
#include "tasks.h"

typedef struct listenerArguments {
	uint16_t portNumber;
} listenerArguments;

void *statisticsThread(void *arg)
{
	while (1)
	{
		printf("############    DBH:%4d\t1K:%5d\t4K:%5d\t1M:%5d\n",
			dbhInUse(),
			buffersInUse(BUFFER1K),
			buffersInUse(BUFFER4K),
			buffersInUse(BUFFER1M));
		sleep(1);
	}
	pthread_exit(NULL);
}

void *dispatcherThread(void *arg)
{
	int sockFD;
	struct sockaddr_in dispatcherAddress;

	sockFD = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFD < 0) {
#ifdef DEBUG
		fprintf(stderr, "Cannot open a socket: %d (%s)\n", errno, strerror(errno));
#endif
		pthread_exit(NULL);
	}

	bzero((char *) &dispatcherAddress, sizeof(dispatcherAddress));

	dispatcherAddress.sin_family = AF_INET;
	dispatcherAddress.sin_addr.s_addr = INADDR_ANY;
	dispatcherAddress.sin_port = htons(12000);

	if (connect(sockFD, (struct sockaddr *)&dispatcherAddress, sizeof(dispatcherAddress)) < 0) {
		close(sockFD);
#ifdef DEBUG
		fprintf(stderr, "Cannot connect to socket: %d (%s)\n", errno, strerror(errno));
#endif
		pthread_exit(NULL);
	}

	pthread_exit(NULL);
}

void *listenerThread(void *arg)
{
	struct listenerArguments *arguments = (struct listenerArguments *)arg;

	int listenSockFD, clientSockFD;
	struct sockaddr_in serverAddress, clientAddress;
	socklen_t clientAddressLength;

	listenSockFD = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSockFD < 0) {
#ifdef DEBUG
		fprintf(stderr, "Cannot open a socket: %d (%s)\n", errno, strerror(errno));
#endif
		pthread_exit(NULL);
	}

	bzero((char *) &serverAddress, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(arguments->portNumber);

	if (bind(listenSockFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
		close(listenSockFD);
#ifdef DEBUG
		fprintf(stderr, "Cannot bind to socket: %d (%s)\n", errno, strerror(errno));
#endif
		pthread_exit(NULL);
	}

	listen(listenSockFD, SOMAXCONN);
	clientAddressLength = sizeof(clientAddress);
	while (1)
	{
		clientSockFD = accept(listenSockFD, (struct sockaddr *)&clientAddress, &clientAddressLength);
		if (clientSockFD < 0) {
#ifdef DEBUG
			fprintf(stderr, "Cannot accept new socket: %d (%s)\n", errno, strerror(errno));
#endif
			sleep(1.0);
			continue;
		}

		char *clientIP = strdup(inet_ntoa(clientAddress.sin_addr));

		struct task *task = startTask(clientSockFD, clientIP);
        if (task == NULL) {
#ifdef DEBUG
            fprintf(stderr, "Cannot start new task\n");
#endif
			close(clientSockFD);
			free(clientIP);
			//sleep(0.2);
            continue;
        }
	}

	close(listenSockFD);

	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	struct listenerArguments arguments;
	pthread_t statisticsHandler;
	pthread_t dispatcherHandler;
	pthread_t listenerHandler;
	int rc;

	if (PQisthreadsafe() != 1)
		exit(-1);

	arguments.portNumber = atoi(argv[1]);

/*
	pthread_attr_t attr;
	rc = pthread_attr_init(&attr);
	if (rc != 0)
		fprintf(stderr, "pthread_attr_init: %d\n", rc);

	int stackSize = 0x800000;
	rc = pthread_attr_setstacksize(&attr, stackSize);
	if (rc != 0)
		fprintf(stderr, "pthread_attr_setstacksize: %d\n", rc);
*/

	constructDB();

	constructBuffers();

	rc = pthread_create(&statisticsHandler, NULL, &statisticsThread, NULL);
    if (rc != 0) {
#ifdef DEBUG
        fprintf(stderr, "Can't create statistics thread: %d (%s)\n", errno, strerror(rc));
#endif
        goto quit;
    }

	rc = pthread_create(&dispatcherHandler, NULL, &dispatcherThread, NULL);
    if (rc != 0) {
#ifdef DEBUG
        fprintf(stderr, "Can't create dispatcher thread: %d (%s)\n", errno, strerror(rc));
#endif
        goto quit;
    }

	rc = pthread_create(&listenerHandler, NULL, &listenerThread, (void *)&arguments);
    if (rc != 0) {
#ifdef DEBUG
        fprintf(stderr, "Can't create listener thread: %d (%s)\n", errno, strerror(rc));
#endif
        goto quit;
    }

	rc = pthread_join(listenerHandler, NULL);
    if (rc != 0) {
#ifdef DEBUG
        fprintf(stderr, "Error has occurred while waiting for listener thread: %d (%s)\n", errno, strerror(rc));
#endif
        goto quit;
    }

quit:
	destructDB();

	destructBuffers();

	return 0;
}
