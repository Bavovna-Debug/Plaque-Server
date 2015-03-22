#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <postgres.h>
#include <arpa/inet.h>
#include "listener.h"

typedef struct dialogue {
	pthread_t			thread;
	int					sockFD;
	struct in_addr      *satelliteAddress;
} dialogue;

void
constructDialogues(void)
{
/*
	struct chain *chain = malloc(sizeof(struct chain) + numberOfConnections * sizeof(int));
	if (chain == NULL) {
        fprintf(stderr, "No memory\n");
        return;
    }
*/
}

void
destructDialogues(void)
{
}

void *
dialogueThread(void *arg)
{
	pthread_exit(NULL);
}

void
dialogueInit(int sockFD, struct in_addr *satelliteAddress)
{
    struct dialogue dialogue;
	int rc = pthread_create(&dialogue.thread, NULL, &dialogueThread, (void *)&dialogue);
    if (rc != 0) {
        //elog(LOG, inet_ntoa(clientAddress.sin_addr));
#ifdef DEBUG
        fprintf(stderr, "Can't create listener thread: %d (%s)\n", errno, strerror(rc));
#endif
        close(sockFD);
    }
}

void *
listenerThread(void *arg)
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
		clientSockFD = accept(listenSockFD, (struct sockaddr *) &clientAddress, &clientAddressLength);
		if (clientSockFD < 0) {
#ifdef DEBUG
			fprintf(stderr, "Cannot accept new socket: %d (%s)\n", errno, strerror(errno));
#endif
			continue;
		}

        dialogueInit(clientSockFD, &clientAddress.sin_addr);
	}

	close(listenSockFD);

	pthread_exit(NULL);
}
