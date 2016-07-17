#include <c.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "chalkboard.h"
#include "paquet.h"
#include "report.h"
#include "tasks.h"
#include "task_kernel.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

// NETWORK_RECEIVE
// NETWORK_SEND

#define FRAGMENT_SIZE		512

#define SECOND									1000
#define MINUTE									60 * SECOND

#define TIMEOUT_ON_POLL_FOR_PILOT				10 * 1000 * 1000	// Milliseconds
#define TIMEOUT_ON_POLL_FOR_PAQUET				10 * 1000 * 1000	// Milliseconds
#define TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT	10 * 1000 * 1000	// Milliseconds
//#define TIMEOUT_ON_WAIT_FOR_RECEIVING			10		    // Seconds for first buffer
//#define TIMEOUT_ON_WAIT_FOR_RECEIVING_KB		 1		    // Seconds per KB
//#define TIMEOUT_ON_WAIT_FOR_TRANSMIT			 1		    // Seconds initial
//#define TIMEOUT_ON_WAIT_FOR_TRANSMIT_4KB		 1		    // Seconds per 4 KB
//#define TIMEOUT_ON_WAIT_FOR_TRANSMIT_MAX		10		    // Seconds maximal

/**
 * FillPaquetWithPilotData()
 *
 * @paquet:
 * @pilot:
 */
inline void
FillPaquetWithPilotData(struct Paquet *paquet, struct PaquetPilot *pilot)
{
	paquet->paquetId = be32toh(pilot->paquetId);
	paquet->commandCode = be32toh(pilot->commandCode);
	paquet->payloadSize = be32toh(pilot->payloadSize);
}

int
ReceiveFixed(
	struct Task *task,
	char *buffer,
	ssize_t expectedSize)
{
	struct pollfd		pollFD;
	int                 sockFD = task->xmit.sockFD;
	ssize_t				receivedPerStep;
	ssize_t				receivedTotal;

    pthread_mutex_lock(&task->xmit.receiveMutex);

	pollFD.fd = sockFD;
	pollFD.events = POLLIN;

	int pollRC = poll(&pollFD, 1, TIMEOUT_ON_POLL_FOR_PILOT);
	if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        pthread_mutex_unlock(&task->xmit.receiveMutex);

		ReportError("Poll error on receive: revents=0x%04X", pollFD.revents);
		SetTaskStatus(task, TaskStatusPollForReceiveFailed);
		return -1;
	}

	if (pollRC == 0) {
        pthread_mutex_unlock(&task->xmit.receiveMutex);

		ReportInfo("Wait for receive timed out");
		SetTaskStatus(task, TaskStatusPollForReceiveTimeout);
		return -1;
	} else if (pollRC != 1) {
        pthread_mutex_unlock(&task->xmit.receiveMutex);

		ReportError("Poll error on receive");
		SetTaskStatus(task, TaskStatusPollForReceiveError);
		return -1;
	}

	receivedTotal = 0;

	do {
		receivedPerStep = read(sockFD,
			buffer + receivedTotal,
			expectedSize - receivedTotal);
		if (receivedPerStep == 0) {
            pthread_mutex_unlock(&task->xmit.receiveMutex);

			ReportError("Nothing read from socket");
			SetTaskStatus(task, TaskStatusNoDataReceived);
			return -1;
		} else if (receivedPerStep == -1) {
            pthread_mutex_unlock(&task->xmit.receiveMutex);

			ReportError("Error reading from socket for fixed data: errno=%d", errno);
			SetTaskStatus(task, TaskStatusReadFromSocketFailed);
			return -1;
		}

		receivedTotal += receivedPerStep;
	} while (receivedTotal < expectedSize);

    pthread_mutex_unlock(&task->xmit.receiveMutex);

	return 0;
}

int
SendFixed(
	struct Task *task,
	char *buffer,
	ssize_t bytesToSend)
{
	struct pollfd		pollFD;
	int                 sockFD = task->xmit.sockFD;
	ssize_t				sentPerStep;
	ssize_t				sentTotal;

    pthread_mutex_lock(&task->xmit.sendMutex);

	pollFD.fd = sockFD;
	pollFD.events = POLLOUT;

	int pollRC = poll(&pollFD, 1, TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT);
	if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        pthread_mutex_unlock(&task->xmit.sendMutex);

		ReportError("Poll error on send: revents=0x%04X", pollFD.revents);
		SetTaskStatus(task, TaskStatusPollForSendFailed);
		return -1;
	}

	if (pollRC == 0) {
        pthread_mutex_unlock(&task->xmit.sendMutex);

		ReportInfo("Wait for send timed out");
		SetTaskStatus(task, TaskStatusPollForSendTimeout);
		return -1;
	} else if (pollRC != 1) {
        pthread_mutex_unlock(&task->xmit.sendMutex);

		ReportError("Poll error on send");
		SetTaskStatus(task, TaskStatusPollForSendError);
		return -1;
	}

	sentTotal = 0;
	do {
		sentPerStep = write(sockFD,
			buffer + sentTotal,
			bytesToSend - sentTotal);
		if (sentPerStep == 0) {
            pthread_mutex_unlock(&task->xmit.sendMutex);

			ReportError("Nothing written to socket");
			SetTaskStatus(task, TaskStatusNoDataSent);
			return -1;
		} else if (sentPerStep == -1) {
            pthread_mutex_unlock(&task->xmit.sendMutex);

			ReportError("Error writing to socket: errno=%d", errno);
			SetTaskStatus(task, TaskStatusWriteToSocketFailed);
			return -1;
		}

		sentTotal += sentPerStep;
	} while (sentTotal < bytesToSend);

    pthread_mutex_unlock(&task->xmit.sendMutex);

	return 0;
}

int
ReceivePaquet(struct Paquet *paquet, struct MMPS_Buffer *receiveBuffer)
{
	struct Task			*task = paquet->task;
	struct pollfd		*pollFD = &paquet->pollFD;
	int                 sockFD = task->xmit.sockFD;
	ssize_t				receivedPerStep;
	ssize_t				receivedPerBuffer;
	ssize_t				receivedTotal;
	ssize_t				toReceivePerBuffer;
	ssize_t				toReceiveTotal;

    pthread_mutex_lock(&task->xmit.receiveMutex);

	pollFD->fd = sockFD;
	pollFD->events = POLLIN;

	int pollRC = poll(pollFD, 1, TIMEOUT_ON_POLL_FOR_PAQUET);
	if (pollFD->revents & (POLLERR | POLLHUP | POLLNVAL))
	{
        pthread_mutex_unlock(&task->xmit.receiveMutex);

		ReportError("Poll error on receive: revents=0x%04X", pollFD->revents);
		SetTaskStatus(task, TaskStatusPollForReceiveFailed);
		return -1;
	}

	if (pollRC == 0)
	{
        pthread_mutex_unlock(&task->xmit.receiveMutex);

		ReportInfo("Wait for receive timed out");
		SetTaskStatus(task, TaskStatusPollForReceiveTimeout);
		return -1;
	}
	else if (pollRC != 1)
	{
        pthread_mutex_unlock(&task->xmit.receiveMutex);

		ReportError("Poll error on receive");
		SetTaskStatus(task, TaskStatusPollForReceiveError);
		return -1;
	}

	struct PaquetPilot *pilot = NULL;

	struct MMPS_Buffer *buffer = receiveBuffer;

	// In case there is data already in receive buffer left from previous read...
	//
	receivedTotal = buffer->dataSize;
	toReceiveTotal = buffer->bufferSize - buffer->dataSize;
	receivedPerBuffer = buffer->dataSize;

	while (1)
	{
		toReceivePerBuffer = toReceiveTotal - receivedTotal;

		if (toReceivePerBuffer > buffer->bufferSize - receivedPerBuffer)
			toReceivePerBuffer = buffer->bufferSize - receivedPerBuffer;

		do {
			receivedPerStep = read(sockFD,
				buffer->data + receivedPerBuffer,
				toReceivePerBuffer - receivedPerBuffer);
			if (receivedPerStep == 0)
			{
                pthread_mutex_unlock(&task->xmit.receiveMutex);

				ReportError("Nothing read from socket");
				SetTaskStatus(task, TaskStatusNoDataReceived);
				return -1;
			}
			else if (receivedPerStep == -1)
			{
                pthread_mutex_unlock(&task->xmit.receiveMutex);

				ReportError("Error reading from socket for paquet: errno=%d", errno);
				SetTaskStatus(task, TaskStatusReadFromSocketFailed);
				return -1;
			}

			receivedPerBuffer += receivedPerStep;

			if ((pilot == NULL) && (toReceivePerBuffer >= sizeof(struct PaquetPilot)))
			{
				pilot = (struct PaquetPilot *)buffer->data;

				FillPaquetWithPilotData(paquet, pilot);

				uint64 signature = be64toh(pilot->signature);
				if (signature != API_PaquetSignature)
				{
                    pthread_mutex_unlock(&task->xmit.receiveMutex);

					ReportError("No valid paquet signature: 0x%016lX", signature);
					SetTaskStatus(task, TaskStatusMissingPaquetSignature);
					return -1;
				}

				toReceiveTotal = sizeof(struct PaquetPilot) + paquet->payloadSize;

				if (paquet->payloadSize < buffer->bufferSize)
					toReceivePerBuffer = paquet->payloadSize;
			}
		} while (receivedPerBuffer < toReceivePerBuffer);

		buffer->dataSize = receivedPerBuffer;

		receivedTotal += receivedPerBuffer;

		if (receivedTotal < toReceiveTotal)
		{
		    struct MMPS_Pool *pool = chalkboard->pools.dynamic;
			struct MMPS_Buffer *nextBuffer = MMPS_PeekBufferOfSize(pool, toReceiveTotal - receivedTotal, BUFFER_XMIT);
			if (nextBuffer == NULL)
			{
                pthread_mutex_unlock(&task->xmit.receiveMutex);

				ReportError("Cannot extend buffer");
				SetTaskStatus(task, TaskStatusCannotExtendBufferForInput);
				return -1;
			}

			buffer->next = nextBuffer;
			buffer = nextBuffer;
		} else {
			break;
		}

		receivedPerBuffer = 0;
	}

	if (receivedTotal < sizeof(struct PaquetPilot))
	{
        pthread_mutex_unlock(&task->xmit.receiveMutex);

		ReportError("Missing paquet pilot (received %d bytes only)", (int)receivedTotal);
		SetTaskStatus(task, TaskStatusMissingPaquetPilot);
		return -1;
	}

    pthread_mutex_unlock(&task->xmit.receiveMutex);

	return 0;
}

int
SendPaquet(struct Paquet *paquet)
{
	struct Task			*task = paquet->task;
	struct pollfd		*pollFD = &paquet->pollFD;
	int                 sockFD = task->xmit.sockFD;
	size_t				toSendPerBuffer;
	ssize_t				sentPerStep;
	ssize_t				sentPerBuffer;

    pthread_mutex_lock(&task->xmit.sendMutex);

	pollFD->fd = sockFD;
	pollFD->events = POLLOUT;

	int pollRC = poll(pollFD, 1, TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT);
	if (pollFD->revents & (POLLERR | POLLHUP | POLLNVAL)) {
        pthread_mutex_unlock(&task->xmit.sendMutex);

		ReportError("Poll error on send: 0x%04X", pollFD->revents);
		SetTaskStatus(task, TaskStatusPollForSendFailed);
		return -1;
	}

	if (pollRC == 0) {
        pthread_mutex_unlock(&task->xmit.sendMutex);

		ReportInfo("Wait for send timed out");
		SetTaskStatus(task, TaskStatusPollForSendTimeout);
		return -1;
	} else if (pollRC != 1) {
        pthread_mutex_unlock(&task->xmit.sendMutex);

		ReportError("Poll error on send");
		SetTaskStatus(task, TaskStatusPollForSendError);
		return -1;
	}

	struct MMPS_Buffer *buffer = paquet->outputBuffer;
	if (buffer == NULL) {
        pthread_mutex_unlock(&task->xmit.sendMutex);

		ReportError("No output buffer provided");
		SetTaskStatus(task, TaskStatusNoOutputDataProvided);
		return -1;
	}

	struct PaquetPilot *pilot = (struct PaquetPilot *) buffer->data;
	pilot->signature = htobe64(API_PaquetSignature);
	pilot->paquetId = htobe32(paquet->paquetId);
	pilot->commandCode = htobe32(paquet->commandCode);
	pilot->payloadSize = htobe32(MMPS_TotalDataSize(buffer) - sizeof(struct PaquetPilot));

	do {
		toSendPerBuffer = buffer->dataSize;
		sentPerBuffer = 0;
		do {
			sentPerStep = write(sockFD,
			    buffer->data + sentPerBuffer,
			    toSendPerBuffer);
			if (sentPerStep == 0) {
                pthread_mutex_unlock(&task->xmit.sendMutex);

				ReportError("Nothing written to socket");
				SetTaskStatus(task, TaskStatusNoDataSent);
				return -1;
			} else if (sentPerStep == -1) {
                pthread_mutex_unlock(&task->xmit.sendMutex);

				ReportError("Error writing to socket: errno=%d", errno);
				SetTaskStatus(task, TaskStatusWriteToSocketFailed);
				return -1;
			}

			sentPerBuffer += sentPerStep;
		} while (sentPerBuffer < toSendPerBuffer);

		buffer = buffer->next;
	} while (buffer != NULL);

    pthread_mutex_unlock(&task->xmit.sendMutex);

	return 0;
}
