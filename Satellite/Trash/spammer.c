#define NUMBER_OF_THREADS	20
#define PAYLOAD_MIN		200
#define PAYLOAD_MAX		1024

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>

#define PORT_NUMBER 	12001

#define BYTES_TO_RECEIVE	50
#define FRAGMENT_SIZE		512

#define MAX(a, b) (a > b) ? a : b

static pthread_t threads[NUMBER_OF_THREADS];

static uint16_t portNumber;

#define BONJOUR_ID			0xD9E4D6D1D5D6C200
#define BONSOIR_ID			0x00C2D6D5E2D6C9D9

#pragma pack(push, 1)
typedef struct request {
	uint64_t		projectId;
	char			deviceToken[16];
	uint32_t		commandCode;
	uint32_t		payloadSize;
} request;
#pragma pack(pop)

void *task(void *arg)
{
	int sockFD;
	const char *ipstr = "192.168.178.35";
	struct in_addr ip;
	struct hostent *server;
	struct sockaddr_in serverAddress;
	char *buffer;
	struct request *request;
	ssize_t		bytesReadPerStep;
	ssize_t		bytesReadTotal;
	int 		bufferLength;
	int 		bufferOffset;
	size_t		bytesToSend;
	ssize_t		bytesSent;
	int			payloadSize;

	buffer = malloc(MAX(sizeof(struct request) + PAYLOAD_MAX, BYTES_TO_RECEIVE));
	if (buffer == NULL)
		fprintf(stderr, "No memory\n");
	request = (struct request *)buffer;

	if (!inet_aton(ipstr, &ip)) {
		fprintf(stderr, "Cannot parse address");
		return NULL;
	}

	server = gethostbyaddr((const void *)&ip, sizeof ip, AF_INET);
	if (server == NULL) {
		fprintf(stderr, "No such server");
		return NULL;
	}

	bzero((char *) &serverAddress, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(portNumber);
	bcopy((char *)server->h_addr, (char *)&serverAddress.sin_addr.s_addr, server->h_length);

	while (1)
	{
		payloadSize = PAYLOAD_MIN + ((random() & (PAYLOAD_MAX - 1)) - PAYLOAD_MIN);
		request->projectId = BONJOUR_ID;
		request->payloadSize = payloadSize;

		sockFD = socket(AF_INET, SOCK_STREAM, 0);
		if (sockFD < 0) {
			fprintf(stderr, "Cannot open a socket");
			return NULL;
		}

		if (connect(sockFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
			fprintf(stderr, "ERROR connecting");
			return NULL;
		}

//if (random() & 1) sleep(1);
		bufferLength = sizeof(struct request) + payloadSize;
		bufferOffset = 0;
		do {
			bytesToSend = bufferLength - bufferOffset;
			if (bytesToSend > FRAGMENT_SIZE)
				bytesToSend = FRAGMENT_SIZE;

			bytesSent = write(sockFD, buffer + bufferOffset, bytesToSend);
			if (bytesSent < 0) {
				fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
				break;
			}

			bufferOffset += bytesSent;
		} while (bufferOffset < bufferLength);

//if (random() & 1) sleep(2);

		bytesReadTotal = 0;
		do {
			bytesReadPerStep = read(sockFD, buffer + bytesReadTotal, payloadSize - bytesReadTotal);
			if (bytesReadPerStep < 0) {
				fprintf(stderr, "Error reading request: %s\n", strerror(errno));
				break;
			}

			bytesReadTotal += bytesReadPerStep;

		  	//printf("%10d  received (%d / %d) bytes\n", sockFD, (int)bytesReadPerStep, (int)bytesReadTotal);

			if (bytesReadPerStep == 0)
				break;
		} while (bytesReadTotal < payloadSize);

		close(sockFD);

		printf("%8d  %s\n", payloadSize, buffer);
		//sleep(0.2);
	}

    return NULL;
}

int main(int argc, char *argv[])
{
	portNumber = atoi(argv[1]);

	pthread_attr_t attr;
	int rc;
	rc = pthread_attr_init(&attr);
	if (rc != 0)
		fprintf(stderr, "pthread_attr_init: %d\n", rc);

	int stackSize = 0x800000;
	rc = pthread_attr_setstacksize(&attr, stackSize);
	if (rc != 0)
		fprintf(stderr, "pthread_attr_setstacksize: %d\n", rc);

	int i;
    for (i = 0; i < NUMBER_OF_THREADS; i++)
    {
        int err = pthread_create(&(threads[i]), NULL, &task, (void *)i);
        if (err == 0)
        	printf("Thread opened - %d\n", i);
        else
            printf("Cannot open thread: %s\n", strerror(err));
    }

	sleep(10000);

	return 0;
}
