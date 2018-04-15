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
#include "task_xmit.h"

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

#ifdef DUPLEX
#define ReceiveMutexLock(task)          pthread_mutex_lock(&task->xmit.receiveMutex);
#define ReceiveMutexUnlock(task)        pthread_mutex_unlock(&task->xmit.receiveMutex);
#define SendMutexLock(task)             pthread_mutex_lock(&task->xmit.sendMutex);
#define SendMutexUnlock(task)           pthread_mutex_unlock(&task->xmit.sendMutex);
#else
#define ReceiveMutexLock(task)          pthread_mutex_lock(&task->xmit.xmitMutex);
#define ReceiveMutexUnlock(task)        pthread_mutex_unlock(&task->xmit.xmitMutex);
#define SendMutexLock(task)             pthread_mutex_lock(&task->xmit.xmitMutex);
#define SendMutexUnlock(task)           pthread_mutex_unlock(&task->xmit.xmitMutex);
#endif

/**
 * FillPaquetWithPilotData()
 *
 * @paquet:
 * @pilot:
 */
inline void
FillPaquetWithPilotData(struct Paquet *paquet)
{
	struct PaquetPilot *pilot;

	pilot = (struct PaquetPilot *) paquet->pilot;

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

    ReceiveMutexLock(task);

	pollFD.fd = sockFD;
	pollFD.events = POLLIN;

	int pollRC = poll(&pollFD, 1, TIMEOUT_ON_POLL_FOR_PILOT);
	if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        ReceiveMutexUnlock(task);

		ReportError("[Xmit] Poll error on receive: revents=0x%04X", pollFD.revents);
		SetTaskStatus(task, TaskStatusPollForReceiveFailed);
		return -1;
	}

	if (pollRC == 0) {
        ReceiveMutexUnlock(task);

		ReportInfo("[Xmit] Wait for receive timed out");
		SetTaskStatus(task, TaskStatusPollForReceiveTimeout);
		return -1;
	} else if (pollRC != 1) {
        ReceiveMutexUnlock(task);

		ReportError("[Xmit] Poll error on receive");
		SetTaskStatus(task, TaskStatusPollForReceiveError);
		return -1;
	}

	receivedTotal = 0;

	do {
		receivedPerStep = read(sockFD,
			buffer + receivedTotal,
			expectedSize - receivedTotal);
		if (receivedPerStep == 0) {
            ReceiveMutexUnlock(task);

			ReportError("[Xmit] Nothing read from socket");
			SetTaskStatus(task, TaskStatusNoDataReceived);
			return -1;
		} else if (receivedPerStep == -1) {
            ReceiveMutexUnlock(task);

			ReportError("[Xmit] Error reading from socket for fixed data: errno=%d", errno);
			SetTaskStatus(task, TaskStatusReadFromSocketFailed);
			return -1;
		}

		receivedTotal += receivedPerStep;
	} while (receivedTotal < expectedSize);

    ReceiveMutexUnlock(task);

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

    SendMutexLock(task);

	pollFD.fd = sockFD;
	pollFD.events = POLLOUT;

	int pollRC = poll(&pollFD, 1, TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT);
	if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        SendMutexUnlock(task);

		ReportError("[Xmit] Poll error on send: revents=0x%04X", pollFD.revents);
		SetTaskStatus(task, TaskStatusPollForSendFailed);
		return -1;
	}

	if (pollRC == 0) {
        SendMutexUnlock(task);

		ReportInfo("[Xmit] Wait for send timed out");
		SetTaskStatus(task, TaskStatusPollForSendTimeout);
		return -1;
	} else if (pollRC != 1) {
        SendMutexUnlock(task);

		ReportError("[Xmit] Poll error on send");
		SetTaskStatus(task, TaskStatusPollForSendError);
		return -1;
	}

	sentTotal = 0;
	do {
		sentPerStep = write(sockFD,
			buffer + sentTotal,
			bytesToSend - sentTotal);
		if (sentPerStep == 0) {
            SendMutexUnlock(task);

			ReportError("[Xmit] Nothing written to socket");
			SetTaskStatus(task, TaskStatusNoDataSent);
			return -1;
		} else if (sentPerStep == -1) {
            SendMutexUnlock(task);

			ReportError("[Xmit] Error writing to socket: errno=%d", errno);
			SetTaskStatus(task, TaskStatusWriteToSocketFailed);
			return -1;
		}

		sentTotal += sentPerStep;
	} while (sentTotal < bytesToSend);

    SendMutexUnlock(task);

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

    ReceiveMutexLock(task);

	pollFD->fd = sockFD;
	pollFD->events = POLLIN;

	int pollRC = poll(pollFD, 1, TIMEOUT_ON_POLL_FOR_PAQUET);
	if (pollFD->revents & (POLLERR | POLLHUP | POLLNVAL))
	{
        ReceiveMutexUnlock(task);

		ReportError("[Xmit] Poll error on receive: revents=0x%04X", pollFD->revents);
		SetTaskStatus(task, TaskStatusPollForReceiveFailed);
		return -1;
	}

	if (pollRC == 0)
	{
        ReceiveMutexUnlock(task);

		ReportInfo("[Xmit] Wait for receive timed out");
		SetTaskStatus(task, TaskStatusPollForReceiveTimeout);
		return -1;
	}
	else if (pollRC != 1)
	{
        ReceiveMutexUnlock(task);

		ReportError("[Xmit] Poll error on receive");
		SetTaskStatus(task, TaskStatusPollForReceiveError);
		return -1;
	}

	struct PaquetPilot *pilot = NULL;

	struct MMPS_Buffer *buffer = receiveBuffer;

#if 1

	pilot = (struct PaquetPilot *) paquet->pilot;

	struct msghdr       message;
    struct iovec        iov[2];

    message.msg_name       = NULL;
    message.msg_namelen    = 0;
    message.msg_iov        = &iov[0];

    message.msg_iovlen     = 1;
    message.msg_control    = NULL;
    message.msg_controllen = 0;
    iov[0].iov_len = sizeof(struct PaquetPilot);
    iov[0].iov_base = pilot;
	receivedTotal = recvmsg(sockFD, &message, 0);
    if (receivedTotal < 0)
    {
        ReceiveMutexUnlock(task);

        ReportError("[Xmit] Error has occurred on receive: errno=%d", errno);

        return -1;
    }

    // If nothing is received, then the connection is closed.
    // Just quit, let the main socket be able to accept new connection.
    //
    if (receivedTotal == 0)
    {
        ReceiveMutexUnlock(task);

        ReportInfo("[Xmit] Connection lost");

        return -1;
    }

    ReportInfo("[Xmit] Received %lu bytes", receivedTotal);

	FillPaquetWithPilotData(paquet);

	uint64 signature = be64toh(pilot->signature);
	if (signature != API_PaquetSignature)
	{
        ReceiveMutexUnlock(task);

		ReportError("[Xmit] No valid paquet signature: 0x%016lX", signature);
		SetTaskStatus(task, TaskStatusMissingPaquetSignature);
		return -1;
	}

    iov[0].iov_len = paquet->payloadSize;
    iov[0].iov_base = buffer->data;
	receivedTotal = recvmsg(sockFD, &message, 0);
    if (receivedTotal < 0)
    {
        ReceiveMutexUnlock(task);

        ReportError("[Xmit] Error has occurred on receive: errno=%d", errno);

        return -1;
    }

    // If nothing is received, then the connection is closed.
    // Just quit, let the main socket be able to accept new connection.
    //
    if (receivedTotal == 0)
    {
        ReceiveMutexUnlock(task);

        ReportInfo("[Xmit] Connection lost");

        return -1;
    }

    ReportInfo("[Xmit] Received %lu bytes", receivedTotal);

    buffer->dataSize = receivedTotal;

#else

	// In case there is data already in receive buffer left from previous read...
	//
	receivedTotal = buffer->dataSize;
	toReceiveTotal = buffer->bufferSize - buffer->dataSize;
	receivedPerBuffer = buffer->dataSize;

	for (;;)
	{
		toReceivePerBuffer = toReceiveTotal - receivedTotal;

		if (toReceivePerBuffer > buffer->bufferSize - receivedPerBuffer)
			toReceivePerBuffer = buffer->bufferSize - receivedPerBuffer;

		do
		{
			receivedPerStep = read(sockFD,
				buffer->data + receivedPerBuffer,
				toReceivePerBuffer - receivedPerBuffer);
			if (receivedPerStep == 0)
			{
                ReceiveMutexUnlock(task);

				ReportError("[Xmit] Nothing read from socket");
				SetTaskStatus(task, TaskStatusNoDataReceived);
				return -1;
			}
			else if (receivedPerStep == -1)
			{
                ReceiveMutexUnlock(task);

				ReportError("[Xmit] Error reading from socket for paquet: errno=%d", errno);
				SetTaskStatus(task, TaskStatusReadFromSocketFailed);
				return -1;
			}

			receivedPerBuffer += receivedPerStep;

			if ((pilot == NULL) && (toReceivePerBuffer >= sizeof(struct PaquetPilot)))
			{
				pilot = (struct PaquetPilot *) paquet->pilot;

                memcpy(pilot, buffer->data, sizeof(struct PaquetPilot));

                if (receivedPerBuffer > sizeof(struct PaquetPilot))
                {
                    memcpy(buffer->data,
                            buffer->data + sizeof(struct PaquetPilot),
                            receivedPerBuffer - sizeof(struct PaquetPilot));
                }

				FillPaquetWithPilotData(paquet);

				uint64 signature = be64toh(pilot->signature);
				if (signature != API_PaquetSignature)
				{
                    ReceiveMutexUnlock(task);

					ReportError("[Xmit] No valid paquet signature: 0x%016lX", signature);
					SetTaskStatus(task, TaskStatusMissingPaquetSignature);
					return -1;
				}

				toReceiveTotal = sizeof(struct PaquetPilot) + paquet->payloadSize;

				if (paquet->payloadSize < buffer->bufferSize)
					toReceivePerBuffer = paquet->payloadSize;
			}
		}
		while (receivedPerBuffer < toReceivePerBuffer);

		buffer->dataSize = receivedPerBuffer;

		receivedTotal += receivedPerBuffer;

		if (receivedTotal < toReceiveTotal)
		{
		    struct MMPS_Pool *pool = chalkboard->pools.dynamic;
			struct MMPS_Buffer *nextBuffer =
			    MMPS_PeekBufferOfSize(pool, toReceiveTotal - receivedTotal, BUFFER_XMIT);

			if (nextBuffer == NULL)
			{
                ReceiveMutexUnlock(task);

				ReportError("[Xmit] Cannot extend buffer");
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
        ReceiveMutexUnlock(task);

		ReportError("[Xmit] Missing paquet pilot (received %d bytes only)", (int)receivedTotal);
		SetTaskStatus(task, TaskStatusMissingPaquetPilot);
		return -1;
	}

#endif

    ReceiveMutexUnlock(task);

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

    SendMutexLock(task);

	pollFD->fd = sockFD;
	pollFD->events = POLLOUT;

	int pollRC = poll(pollFD, 1, TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT);
	if (pollFD->revents & (POLLERR | POLLHUP | POLLNVAL)) {
        SendMutexUnlock(task);

		ReportError("[Xmit] Poll error on send: 0x%04X", pollFD->revents);
		SetTaskStatus(task, TaskStatusPollForSendFailed);
		return -1;
	}

	if (pollRC == 0) {
        SendMutexUnlock(task);

		ReportInfo("[Xmit] Wait for send timed out");
		SetTaskStatus(task, TaskStatusPollForSendTimeout);
		return -1;
	} else if (pollRC != 1) {
        SendMutexUnlock(task);

		ReportError("[Xmit] Poll error on send");
		SetTaskStatus(task, TaskStatusPollForSendError);
		return -1;
	}

	struct MMPS_Buffer *buffer = paquet->outputBuffer;
	if (buffer == NULL) {
        SendMutexUnlock(task);

		ReportError("[Xmit] No output buffer provided");
		SetTaskStatus(task, TaskStatusNoOutputDataProvided);
		return -1;
	}

	struct PaquetPilot *pilot = (struct PaquetPilot *) paquet->pilot;

#if 1

	pilot->signature = htobe64(API_PaquetSignature);
	pilot->paquetId = htobe32(paquet->paquetId);
	pilot->commandCode = htobe32(paquet->commandCode);
	pilot->payloadSize = htobe32(MMPS_TotalDataSize(buffer));

	struct msghdr       message;
    struct iovec        iov[XMIT_MAX_NUMBER_OF_VECTORS];

    message.msg_name       = NULL;
    message.msg_namelen    = 0;
    message.msg_iov        = &iov[0];

    message.msg_iovlen     = 1;
    message.msg_control    = NULL;
    message.msg_controllen = 0;
    iov[0].iov_len = sizeof(struct PaquetPilot);
    iov[0].iov_base = pilot;

    if (MMPS_TotalDataSize(buffer) > 0)
    {
        unsigned int vectorId = 1;
        while (buffer != NULL)
        {
        	if (buffer->dataSize == 0)
        		break;

        	iov[vectorId].iov_len = buffer->dataSize;
        	iov[vectorId].iov_base = buffer->data;
        	vectorId++;
        	message.msg_iovlen++;
        	buffer = buffer->next;

            if (message.msg_iovlen == XMIT_MAX_NUMBER_OF_VECTORS)
            {
                break;
            }
        }
    }

	sentPerStep = sendmsg(sockFD, &message, 0);
    if (sentPerStep < 0)
    {
        ReceiveMutexUnlock(task);

        ReportError("[Xmit] Error has occurred on send: errno=%d", errno);

        return -1;
    }

    ReportInfo("[Xmit] Sent %ld bytes", sentPerStep);

#else

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
                SendMutexUnlock(task);

				ReportError("[Xmit] Nothing written to socket");
				SetTaskStatus(task, TaskStatusNoDataSent);
				return -1;
			} else if (sentPerStep == -1) {
                SendMutexUnlock(task);

				ReportError("[Xmit] Error writing to socket: errno=%d", errno);
				SetTaskStatus(task, TaskStatusWriteToSocketFailed);
				return -1;
			}

			sentPerBuffer += sentPerStep;
		} while (sentPerBuffer < toSendPerBuffer);

		buffer = buffer->next;
	} while (buffer != NULL);

#endif

    SendMutexUnlock(task);

	return 0;
}
