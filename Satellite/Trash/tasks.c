#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <poll.h>
#include "bonjour.h"
#include "processor.h"
#include "queue.h"
#include "tasks.h"

#define BYTES_TO_SEND		50
#define FRAGMENT_SIZE		512

#define TIMEOUT_ON_WAIT_FOR_BEGIN_TO_RECEIVE	500		// Nanoseconds
#define TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT	4000	// Nanoseconds
#define TIMEOUT_ON_WAIT_FOR_RECEIVING			4		// Seconds for first buffer
#define TIMEOUT_ON_WAIT_FOR_RECEIVING_KB		1		// Seconds per KB
#define TIMEOUT_ON_WAIT_FOR_TRANSMIT			1		// Seconds initial
#define TIMEOUT_ON_WAIT_FOR_TRANSMIT_4KB		1		// Seconds per 4KB
#define TIMEOUT_ON_WAIT_FOR_TRANSMIT_MAX		10		// Seconds maximal

void *receiverThread(void *arg)
{
	struct receiver		*receiver = (struct receiver *)arg;
	struct pollfd		pollFD;
	ssize_t				receivedPerStep;
	ssize_t				receivedPerBuffer;
	ssize_t				receivedTotal;
	ssize_t				toReceivePerBuffer;
	ssize_t				toReceiveTotal;

	while (1)
	{
		sem_wait(&receiver->waitForReadyToGo);

		struct buffer *mainBuffer = peekBuffer(KB);
		if (mainBuffer == NULL) {
			sem_post(&receiver->waitForComplete);
			continue;
		}

		pollFD.fd = receiver->task->sockFD;
		pollFD.events = POLLIN;

		int pollRC = poll(&pollFD, 1, TIMEOUT_ON_WAIT_FOR_BEGIN_TO_RECEIVE);
		if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "Poll error on receive: %d\n", pollFD.revents);
			pokeBuffer(mainBuffer);
			sem_post(&receiver->waitForComplete);
			continue;
		}

		if (pollRC == 1) {
			int sockFD = receiver->task->sockFD;
			struct bonjourPilot *pilot = NULL;

			struct buffer *buffer = mainBuffer;

			receivedTotal = 0;
			toReceiveTotal = buffer->bufferSize;

			while (1)
			{
				receivedPerBuffer = 0;

				toReceivePerBuffer = toReceiveTotal - receivedTotal;

				if (toReceivePerBuffer > buffer->bufferSize)
					toReceivePerBuffer = buffer->bufferSize;

				do {
					receivedPerStep = read(sockFD, buffer->data + receivedPerBuffer, toReceivePerBuffer - receivedPerBuffer);
					if (receivedPerStep == 0) {
						fprintf(stderr, "Nothing read from socket\n");
						pokeBuffer(mainBuffer);
						//sem_post(&receiver->waitForComplete);
						goto error;
					} else if (receivedPerStep == -1) {
						fprintf(stderr, "Error reading from socket: %s\n", strerror(errno));
						pokeBuffer(mainBuffer);
						//sem_post(&receiver->waitForComplete);
						goto error;
					}

					receivedPerBuffer += receivedPerStep;

					if ((pilot == NULL) && (receivedPerBuffer >= sizeof(struct bonjourPilot))) {
						pilot = (struct bonjourPilot *)buffer->data;

						uint64_t projectId = be64toh(pilot->projectId);
						uint32_t payloadSize = bonjourGetPayloadSize(buffer);

						if (projectId != BONJOUR_ID) {
							fprintf(stderr, "No valid bonjour signature\n");
							pokeBuffer(mainBuffer);
							//sem_post(&receiver->waitForComplete);
							goto error;
						}

						toReceiveTotal = payloadSize;

						if (payloadSize < buffer->bufferSize)
							toReceivePerBuffer = payloadSize;
					}
				} while (receivedPerBuffer < toReceivePerBuffer);

				buffer->dataSize = receivedPerBuffer;

				receivedTotal += receivedPerBuffer;

				if (receivedTotal < toReceiveTotal) {
					struct buffer *nextBuffer = peekBuffer(toReceiveTotal - receivedTotal);
					if (nextBuffer == NULL) {
						pokeBuffer(mainBuffer);
						//sem_post(&receiver->waitForComplete);
						goto error;
					}

					buffer->next = nextBuffer;
					buffer = nextBuffer;
				} else {
					break;
				}
			}

			if (receivedTotal < sizeof(struct bonjourPilot)) {
				fprintf(stderr, "Missing bonjour pilot (received %d bytes only)\n", (int)receivedTotal);
				pokeBuffer(mainBuffer);
				//sem_post(&receiver->waitForComplete);
				goto error;
			}
		} else if (pollRC == 0) {
			fprintf(stderr, "Wait for read timed out\n");
		} else {
			fprintf(stderr, "Poll error on receive\n");
		}

		receiver->task->request = mainBuffer;

error:
		sem_post(&receiver->waitForComplete);
	}

	return NULL;
}

void *transmitterThread(void *arg)
{
	struct transmitter	*transmitter = (struct transmitter *)arg;
	struct pollfd		pollFD;
	size_t				toSendPerBuffer;
	ssize_t				sentPerStep;
	ssize_t				sentPerBuffer;

	while (1)
	{
		sem_wait(&transmitter->waitForReadyToGo);

		pollFD.fd = transmitter->task->sockFD;
		pollFD.events = POLLOUT;

		int pollRC = poll(&pollFD, 1, TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT);
		if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "Poll error on receive: %d\n", pollFD.revents);
			sem_post(&transmitter->waitForComplete);
			continue;
		}

		if (pollRC == 1) {
			int sockFD = transmitter->task->sockFD;

			struct buffer *buffer = transmitter->task->response;

			bonjourSetPayloadSize(buffer, totalDataSize(buffer) - sizeof(struct bonjourPilot));

			do {
				toSendPerBuffer = buffer->dataSize;
				sentPerBuffer = 0;
				do {
					sentPerStep = write(sockFD, buffer->data + sentPerBuffer, toSendPerBuffer);
					if (sentPerStep == 0) {
						fprintf(stderr, "Nothing written to socket\n");
						sem_post(&transmitter->waitForComplete);
						goto error;
					} else if (sentPerStep == -1) {
						fprintf(stderr, "Error writing to socket: %s\n", strerror(errno));
						sem_post(&transmitter->waitForComplete);
						goto error;
					}

					sentPerBuffer += sentPerStep;
				} while (sentPerBuffer < toSendPerBuffer);

				buffer = buffer->next;
			} while (buffer != NULL);
		} else if (pollRC == 0) {
			fprintf(stderr, "Wait for send timed out\n");
		} else {
			fprintf(stderr, "Poll error on send\n");
		}

error:
		sem_post(&transmitter->waitForComplete);
	}

	return NULL;
}

void *taskThread(void *arg)
{
	struct task	*task = (struct task *)arg;

	while (1) {
		sem_wait(&task->waitForJob);

		struct timespec ts;
		int rc;

		rc = clock_gettime(CLOCK_REALTIME_COARSE, &ts);
		if (rc == -1) {
			fprintf(stderr, "Cannot get system time: %s\n", strerror(errno));

			close(task->sockFD);

			pokeTask(task);

			continue;
		}

	    ts.tv_sec += TIMEOUT_ON_WAIT_FOR_RECEIVING;

		struct receiver *receiver = peekReceiver(task);
		if (receiver == NULL) {
			fprintf(stderr, "No free receiver\n");

			close(task->sockFD);

			pokeTask(task);

			continue;
		}

		rc = sem_timedwait(&receiver->waitForComplete, &ts);
		if ((rc == -1) && (errno == ETIMEDOUT)) {
			fprintf(stderr, "Timed out receive\n");

			close(task->sockFD);

			pokeReceiver(receiver);

			pokeTask(task);

			continue;
		}

		pokeReceiver(receiver);

		if (task->request == NULL) {
			fprintf(stderr, "Nothing received\n");
			close(task->sockFD);

			pokeTask(task);

			continue;
		}

		rc = processBonjour(task);
		if (rc != 0) {
			close(task->sockFD);

			pokeTask(task);

			continue;
		}

		int timeout = TIMEOUT_ON_WAIT_FOR_TRANSMIT;
		timeout += (totalDataSize(task->response) >> 12) * TIMEOUT_ON_WAIT_FOR_TRANSMIT_4KB;
		if (timeout > TIMEOUT_ON_WAIT_FOR_TRANSMIT_MAX)
			timeout = TIMEOUT_ON_WAIT_FOR_TRANSMIT_MAX;

		rc = clock_gettime(CLOCK_REALTIME_COARSE, &ts);
		if (rc == -1) {
			fprintf(stderr, "Cannot get system time: %s\n", strerror(errno));

			close(task->sockFD);

			pokeTask(task);

			continue;
		}

	    ts.tv_sec += timeout;

		struct transmitter *transmitter = peekTransmitter(task);
		if (transmitter == NULL) {
			fprintf(stderr, "No free transmitter\n");

			close(task->sockFD);

			pokeTask(task);

			continue;
		}

		rc = sem_timedwait(&transmitter->waitForComplete, &ts);
		if ((rc == -1) && (errno == ETIMEDOUT)) {
			fprintf(stderr, "Timed out send\n");

			close(task->sockFD);

			pokeTransmitter(transmitter);

			pokeTask(task);

			continue;
		}

		close(task->sockFD);

		pokeTransmitter(transmitter);

		pokeTask(task);
	}

	return NULL;
}
