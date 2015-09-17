#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "broadcaster_api.h"
#include "broadcaster.h"
#include "desk.h"
#include "report.h"

void *
broadcasterThread(void *arg)
{
	struct desk *desk = (struct desk *)arg;
	int sockFD;
	struct sockaddr_in broadcasterddress;

	sockFD = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFD < 0) {
		reportError("Cannot open a socket: errno=%d", errno);
		pthread_exit(NULL);
	}

	bzero((char *)&broadcasterddress, sizeof(broadcasterddress));

	broadcasterddress.sin_family = AF_INET;
	broadcasterddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	broadcasterddress.sin_port = htons(desk->broadcaster.portNumber);

	if (connect(sockFD, (struct sockaddr *)&broadcasterddress, sizeof(broadcasterddress)) < 0) {
		close(sockFD);
		reportError("Cannot connect to socket: errno=%d", errno);
		pthread_exit(NULL);
	}

	pthread_exit(NULL);
}
