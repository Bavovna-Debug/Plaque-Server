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
#include "anticipant.h"
#include "api.h"
#include "paquet.h"
#include "plaques_edit.h"
#include "plaques_query.h"
#include "session.h"
#include "tasks.h"

#define DEBUG

#define FRAGMENT_SIZE		512

#define SECOND									1000
#define MINUTE									60 * SECOND

#define TIMEOUT_ON_POLL_FOR_PILOT				5 * SECOND	// Nanoseconds
#define TIMEOUT_ON_POLL_FOR_PAQUET				5 * MINUTE	// Nanoseconds
#define TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT	4 * SECOND	// Nanoseconds
//#define TIMEOUT_ON_WAIT_FOR_RECEIVING			10		// Seconds for first buffer
//#define TIMEOUT_ON_WAIT_FOR_RECEIVING_KB		1		// Seconds per KB
//#define TIMEOUT_ON_WAIT_FOR_TRANSMIT			1		// Seconds initial
//#define TIMEOUT_ON_WAIT_FOR_TRANSMIT_4KB		1		// Seconds per 4KB
//#define TIMEOUT_ON_WAIT_FOR_TRANSMIT_MAX		10		// Seconds maximal

inline void setTaskStatus(struct task *task, int statusMask)
{
	pthread_spin_lock(&task->statusLock);
	task->status |= statusMask;
	pthread_spin_unlock(&task->statusLock);
}

inline int getTaskStatus(struct task *task)
{
	pthread_spin_lock(&task->statusLock);
	int status = task->status;
	pthread_spin_unlock(&task->statusLock);
	return status;
}

inline void fillPaquetWithPilotData(struct paquet *paquet, struct paquetPilot *pilot)
{
	paquet->paquetId = be32toh(pilot->paquetId);
	paquet->commandCode = be32toh(pilot->commandCode);
	paquet->payloadSize = be32toh(pilot->payloadSize);
}

int receiveFixed(struct task *task, char *buffer, ssize_t expectedSize)
{
	struct pollfd		pollFD;
	ssize_t				receivedPerStep;
	ssize_t				receivedTotal;

	pollFD.fd = task->sockFD;
	pollFD.events = POLLIN;

	int pollRC = poll(&pollFD, 1, TIMEOUT_ON_POLL_FOR_PILOT);
	if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
#ifdef DEBUG
		fprintf(stderr, "Poll error on receive: 0x%04X\n", pollFD.revents);
#endif
		setTaskStatus(task, TaskStatusPollForReceiveFailed);
		return -1;
	}

	if (pollRC == 0) {
#ifdef DEBUG
		fprintf(stderr, "Wait for receive timed out\n");
#endif
		setTaskStatus(task, TaskStatusPollForReceiveTimeout);
		return -1;
	} else if (pollRC != 1) {
#ifdef DEBUG
		fprintf(stderr, "Poll error on receive\n");
#endif
		setTaskStatus(task, TaskStatusPollForReceiveError);
		return -1;
	}

	int sockFD = task->sockFD;

	receivedTotal = 0;

	do {
		receivedPerStep = read(sockFD, buffer + receivedTotal, expectedSize - receivedTotal);
		if (receivedPerStep == 0) {
#ifdef DEBUG
			fprintf(stderr, "Nothing read from socket\n");
#endif
			setTaskStatus(task, TaskStatusNoDataReceived);
			return -1;
		} else if (receivedPerStep == -1) {
#ifdef DEBUG
			fprintf(stderr, "Error reading from socket: %d (%s)\n", errno, strerror(errno));
#endif
			setTaskStatus(task, TaskStatusReadFromSocketFailed);
			return -1;
		}

		receivedTotal += receivedPerStep;
	} while (receivedTotal < expectedSize);

	return 0;
}

int sendFixed(struct task *task, char *buffer, ssize_t bytesToSend)
{
	struct pollfd		pollFD;
	ssize_t				sentPerStep;
	ssize_t				sentTotal;

	pollFD.fd = task->sockFD;
	pollFD.events = POLLOUT;

	int pollRC = poll(&pollFD, 1, TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT);
	if (pollFD.revents & (POLLERR | POLLHUP | POLLNVAL)) {
#ifdef DEBUG
		fprintf(stderr, "Poll error on send: 0x%04X\n", pollFD.revents);
#endif
		setTaskStatus(task, TaskStatusPollForSendFailed);
		return -1;
	}

	if (pollRC == 0) {
#ifdef DEBUG
		fprintf(stderr, "Wait for send timed out\n");
#endif
		setTaskStatus(task, TaskStatusPollForSendTimeout);
		return -1;
	} else if (pollRC != 1) {
#ifdef DEBUG
		fprintf(stderr, "Poll error on receive\n");
#endif
		setTaskStatus(task, TaskStatusPollForSendError);
		return -1;
	}

	sentTotal = 0;
	do {
		sentPerStep = write(task->sockFD, buffer + sentTotal, bytesToSend - sentTotal);
		if (sentPerStep == 0) {
#ifdef DEBUG
			fprintf(stderr, "Nothing written to socket\n");
#endif
			setTaskStatus(task, TaskStatusNoDataSent);
			return -1;
		} else if (sentPerStep == -1) {
#ifdef DEBUG
			fprintf(stderr, "Error writing to socket: %d (%s)\n", errno, strerror(errno));
#endif
			setTaskStatus(task, TaskStatusWriteToSocketFailed);
			return -1;
		}

		sentTotal += sentPerStep;
	} while (sentTotal < bytesToSend);

	return 0;
}

int receivePaquet(struct paquet *paquet, struct buffer *receiveBuffer)
{
	struct task			*task = paquet->task;
	struct pollfd		*pollFD = &paquet->pollFD;
	ssize_t				receivedPerStep;
	ssize_t				receivedPerBuffer;
	ssize_t				receivedTotal;
	ssize_t				toReceivePerBuffer;
	ssize_t				toReceiveTotal;

	pollFD->fd = task->sockFD;
	pollFD->events = POLLIN;

	int pollRC = poll(pollFD, 1, TIMEOUT_ON_POLL_FOR_PAQUET);
	if (pollFD->revents & (POLLERR | POLLHUP | POLLNVAL)) {
#ifdef DEBUG
		fprintf(stderr, "Poll error on receive: 0x%04X\n", pollFD->revents);
#endif
		setTaskStatus(task, TaskStatusPollForReceiveFailed);
		return -1;
	}

	if (pollRC == 0) {
#ifdef DEBUG
		fprintf(stderr, "Wait for receive timed out\n");
#endif
		setTaskStatus(task, TaskStatusPollForReceiveTimeout);
		return -1;
	} else if (pollRC != 1) {
#ifdef DEBUG
		fprintf(stderr, "Poll error on receive\n");
#endif
		setTaskStatus(task, TaskStatusPollForReceiveError);
		return -1;
	}

	struct paquetPilot *pilot = NULL;

	struct buffer *buffer = receiveBuffer;

	// In case there is data already in receive buffer left from previous read...
	//
	receivedTotal = buffer->dataSize;
	toReceiveTotal = buffer->bufferSize - buffer->dataSize;
	receivedPerBuffer = buffer->dataSize;
//printf("START: %d %d\n", receivedTotal, toReceiveTotal);

	while (1)
	{
		toReceivePerBuffer = toReceiveTotal - receivedTotal;

		if (toReceivePerBuffer > buffer->bufferSize - receivedPerBuffer)
			toReceivePerBuffer = buffer->bufferSize - receivedPerBuffer;

		do {
			receivedPerStep = read(task->sockFD, buffer->data + receivedPerBuffer, toReceivePerBuffer - receivedPerBuffer);
			if (receivedPerStep == 0) {
#ifdef DEBUG
				fprintf(stderr, "Nothing read from socket\n");
#endif
				setTaskStatus(task, TaskStatusNoDataReceived);
				return -1;
			} else if (receivedPerStep == -1) {
#ifdef DEBUG
				fprintf(stderr, "Error reading from socket: %d (%s)\n", errno, strerror(errno));
#endif
				setTaskStatus(task, TaskStatusReadFromSocketFailed);
				return -1;
			}

			receivedPerBuffer += receivedPerStep;

			if ((pilot == NULL) && (toReceivePerBuffer >= sizeof(struct paquetPilot))) {
				pilot = (struct paquetPilot *)buffer->data;

				fillPaquetWithPilotData(paquet, pilot);

				uint64_t signature = be64toh(pilot->signature);
				if (signature != PaquetSignature) {
#ifdef DEBUG
					fprintf(stderr, "No valid paquet signature\n");
#endif
					setTaskStatus(task, TaskStatusMissingPaquetSignature);
					return -1;
				}

				toReceiveTotal = sizeof(struct paquetPilot) + paquet->payloadSize;

				if (paquet->payloadSize < buffer->bufferSize)
					toReceivePerBuffer = paquet->payloadSize;
			}
		} while (receivedPerBuffer < toReceivePerBuffer);

		buffer->dataSize = receivedPerBuffer;

		receivedTotal += receivedPerBuffer;

		if (receivedTotal < toReceiveTotal) {
			struct buffer *nextBuffer = peekBuffer(toReceiveTotal - receivedTotal);
			if (nextBuffer == NULL) {
#ifdef DEBUG
				printf("Cannot extend buffer\n");
#endif
				setTaskStatus(task, TaskStatusCannotExtendBufferForInput);
				return -1;
			}

			buffer->next = nextBuffer;
			buffer = nextBuffer;
		} else {
			break;
		}

		receivedPerBuffer = 0;
	}

	if (receivedTotal < sizeof(struct paquetPilot)) {
#ifdef DEBUG
		fprintf(stderr, "Missing paquet pilot (received %d bytes only)\n", (int)receivedTotal);
#endif
		setTaskStatus(task, TaskStatusMissingPaquetPilot);
		return -1;
	}

	return 0;
}

int sendPaquet(struct paquet *paquet)
{
	struct task			*task = paquet->task;
	struct pollfd		*pollFD = &paquet->pollFD;
	size_t				toSendPerBuffer;
	ssize_t				sentPerStep;
	ssize_t				sentPerBuffer;

	pollFD->fd = task->sockFD;
	pollFD->events = POLLOUT;

	int pollRC = poll(pollFD, 1, TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT);
	if (pollFD->revents & (POLLERR | POLLHUP | POLLNVAL)) {
#ifdef DEBUG
		fprintf(stderr, "Poll error on send: 0x%04X\n", pollFD->revents);
#endif
		setTaskStatus(task, TaskStatusPollForSendFailed);
		return -1;
	}

	if (pollRC == 0) {
#ifdef DEBUG
		fprintf(stderr, "Wait for send timed out\n");
#endif
		setTaskStatus(task, TaskStatusPollForSendTimeout);
		return -1;
	} else if (pollRC != 1) {
#ifdef DEBUG
		fprintf(stderr, "Poll error on receive\n");
#endif
		setTaskStatus(task, TaskStatusPollForSendError);
		return -1;
	}

	struct buffer *buffer = paquet->outputBuffer;

	struct paquetPilot *pilot = (struct paquetPilot *)buffer->data;
	pilot->signature = htobe64(PaquetSignature);
	pilot->paquetId = htobe32(paquet->paquetId);
	pilot->commandCode = htobe32(paquet->commandCode);
	pilot->payloadSize = htobe32(totalDataSize(buffer) - sizeof(struct paquetPilot));

	do {
		toSendPerBuffer = buffer->dataSize;
		sentPerBuffer = 0;
		do {
			sentPerStep = write(task->sockFD, buffer->data + sentPerBuffer, toSendPerBuffer);
			if (sentPerStep == 0) {
#ifdef DEBUG
				fprintf(stderr, "Nothing written to socket\n");
#endif
				setTaskStatus(task, TaskStatusNoDataSent);
				return -1;
			} else if (sentPerStep == -1) {
#ifdef DEBUG
				fprintf(stderr, "Error writing to socket: %d (%s)\n", errno, strerror(errno));
#endif
				setTaskStatus(task, TaskStatusWriteToSocketFailed);
				return -1;
			}

			sentPerBuffer += sentPerStep;
		} while (sentPerBuffer < toSendPerBuffer);

		buffer = buffer->next;
	} while (buffer != NULL);

	return 0;
}

void *paquetThread(void *arg)
{
	struct paquet *paquet = (struct paquet *)arg;
	struct task *task = paquet->task;
	int rc;

	switch (paquet->commandCode)
	{
/*
		case CommandAnticipant:
			rc = registerDevice(paquet);
			break;
*/
		case PaquetListOfPlaquesInSight:
			if (pthread_spin_trylock(&task->heavyJobLock) == 0) {
				rc = paquetListOfPlaquesInSight(paquet);
				pthread_spin_unlock(&task->heavyJobLock);
			} else {
				rejectPaquetASBusy(paquet);
			}
			break;

		case PaquetListOfPlaquesOnMap:
			if (pthread_spin_trylock(&task->heavyJobLock) == 0) {
				rc = paquetListOfPlaquesOnMap(paquet);
				pthread_spin_unlock(&task->heavyJobLock);
			} else {
				rejectPaquetASBusy(paquet);
			}
			break;

		case PaquetDownloadPlaquesInSight:
		case PaquetDownloadPlaquesOnMap:
			if (pthread_spin_trylock(&task->downloadLock) == 0) {
				rc = paquetDownloadPlaques(paquet);
				pthread_spin_unlock(&task->downloadLock);
			} else {
				rejectPaquetASBusy(paquet);
			}
			break;

		case PaquetPostNewPlaque:
			rc = paquetPostNewPlaque(paquet);
			break;

		case PaquetPlaqueModifiedLocation:
			rc = paquetChangePlaqueLocation(paquet);
			break;

		case PaquetPlaqueModifiedOrientation:
			rc = paquetChangePlaqueOrientation(paquet);
			break;

		case PaquetPlaqueModifiedSize:
			rc = paquetChangePlaqueSize(paquet);
			break;

		case PaquetPlaqueModifiedColors:
			rc = paquetChangePlaqueColors(paquet);
			break;

		case PaquetPlaqueModifiedFont:
			rc = paquetChangePlaqueFont(paquet);
			break;

		case PaquetPlaqueModifiedInscription:
			rc = paquetChangePlaqueInscription(paquet);
			break;

		case PaquetDownloadProfiles:
			rc = getProfiles(paquet);
			break;

		case PaquetNotificationsToken:
			rc = notificationsToken(paquet);
			break;

		case PaquetValidateProfileName:
			rc = validateProfileName(paquet);
			break;

		case PaquetCreateProfile:
			rc = createProfile(paquet);
			break;

		default:
			printf("Unknown command: 0x%08X\n", paquet->commandCode);
			rc = -1;
	}

	if (rc == 0)
		sendPaquet(paquet);

	if (paquet->inputBuffer == paquet->outputBuffer) {
		if (paquet->inputBuffer != NULL)
			pokeBuffer(paquet->inputBuffer);
	} else {
		if (paquet->inputBuffer != NULL)
			pokeBuffer(paquet->inputBuffer);
		if (paquet->outputBuffer != NULL)
			pokeBuffer(paquet->outputBuffer);
	}

	pthread_exit(NULL);

	return NULL;
}

void dialogueAnticipant(struct task *task)
{
	int rc;

#ifdef DEBUG
        fprintf(stderr, "Anticipant begin\n");
#endif

	struct dialogueAnticipant anticipant;
	rc = receiveFixed(task, (char *)&anticipant, sizeof(anticipant));
	if (rc != 0) {
		setTaskStatus(task, TaskStatusMissingAnticipantRecord);
		return;
	}

	char deviceToken[TokenBinarySize];
	rc = registerDevice(task, &anticipant, (char *)&deviceToken);
	if (rc != 0)
		return;

	rc = sendFixed(task, (char *)&deviceToken, sizeof(deviceToken));
	if (rc != 0)
		return;

#ifdef DEBUG
        fprintf(stderr, "Anticipant end\n");
#endif
}

void dialogueRegular(struct task *task)
{
	int rc;

	struct buffer *receiveBuffer = NULL;

	do {
#ifdef DEBUG
        fprintf(stderr, "Dialoque loop\n");
#endif
		// Allocate memory for new paquet.
		//
		struct paquet *paquet = malloc(sizeof(struct paquet));
		if (paquet == NULL) {
			setTaskStatus(task, TaskStatusOutOfMemory);
			break;
		}

		paquet->task = task;
		paquet->inputBuffer = NULL;
		paquet->outputBuffer = NULL;

		int receiveNeeded;

		// If there is no rest data from previous receive then allocate a new buffer and start receive.
		//
		if (receiveBuffer == NULL) {
			receiveBuffer = peekBuffer(1024);

			if (receiveBuffer == NULL) {
				setTaskStatus(task, TaskStatusOutOfMemory);
				break;
			}

			receiveNeeded = 1;
		} else {
			//
			// If there is rest of data from previous receive available then check
			// whether a complete paquet content is already available.

			// Receive is needed if rest of data does not contain even a paquet pilot.
			//
			if (receiveBuffer->dataSize < sizeof(paquetPilot)) {
				receiveNeeded = 1;
			} else {
				struct paquetPilot *pilot = (struct paquetPilot *)receiveBuffer->data;
				uint32_t payloadSize = be32toh(pilot->payloadSize);

				// Receive is needed if rest of data contains only a part of paquet.
				//
				if (totalDataSize(receiveBuffer) < sizeof(paquetPilot) + payloadSize) {
					receiveNeeded = 1;
				} else {
					//
					// Otherwise complete paquet is available already from previous receive.
					// In such case receive should be skipped.
					//
					receiveNeeded = 0;
					fillPaquetWithPilotData(paquet, pilot);
				}
			}
		}

		if (receiveNeeded != 0) {
			rc = receivePaquet(paquet, receiveBuffer);
			if (rc != 0)
				break;
		}

		int totalReceivedData = totalDataSize(receiveBuffer);

#ifdef DEBUG
		struct paquetPilot *pilot = (struct paquetPilot *)receiveBuffer->data;
		printf(">>>> Paquet %u for command 0x%08X with %d bytes and payload %d bytes\n",
			paquet->paquetId,
			paquet->commandCode,
			totalReceivedData,
			paquet->payloadSize);
#endif

		if (totalReceivedData < sizeof(paquetPilot) + paquet->payloadSize) {
			//
			// If amount of data in receive buffer is less then necessary for current paquet
			// then quit with error.
			//
#ifdef DEBUG
			printf("Received data incomplete\n");
#endif
			setTaskStatus(task, TaskStatusReceivedDataIncomplete);
			pokeBuffer(receiveBuffer);
			receiveBuffer = NULL;
        	break;
		} else if (totalReceivedData == paquet->payloadSize) {
			//
			// If amount of data in receive buffer is exactly what is necessary for current paquet
			// associate receive buffer to paquet and reset pointer to receive buffer so
			// that a new buffer will be allocated on next loop.
			//
			paquet->inputBuffer = receiveBuffer;
			receiveBuffer = NULL;
		} else {
			//
			// If amount of received data is greater than necessary for current paquet
			// then cut off the rest of data from current paquet and move if
			// to a separate buffer, that would be used on next loop.

			// First associate receive buffer to current paquet.
			//
			paquet->inputBuffer = receiveBuffer;

			// Then truncate the buffer.
			//
			struct buffer *sliceDataBuffer = receiveBuffer;
			int paquetDataRest = sizeof(paquetPilot) + paquet->payloadSize;
			while (sliceDataBuffer->next != NULL)
			{
				paquetDataRest -= sliceDataBuffer->dataSize;
				sliceDataBuffer = sliceDataBuffer->next;
			}

			// Allocate new receive buffer to be used on next loop.
			//
			receiveBuffer = peekBuffer(1024);
			if (receiveBuffer == NULL) {
				setTaskStatus(task, TaskStatusOutOfMemory);
				break;
			}

			// Move the rest of data from current paquet to a new receive buffer.
			//
			int sizeOfRestOfData = sliceDataBuffer->dataSize - paquetDataRest;
			memcpy(receiveBuffer->data, sliceDataBuffer->data + paquetDataRest, sizeOfRestOfData);
			receiveBuffer->dataSize = sizeOfRestOfData;

			// Cut the rest of data from current paquet.
			//
			sliceDataBuffer->dataSize = paquetDataRest;
		}

		// Start new thread to process current paquet.
		//
	    rc = pthread_create(&paquet->thread, NULL, &paquetThread, paquet);
    	if (rc != 0) {
#ifdef DEBUG
        	fprintf(stderr, "Can't create paquet thread: %d (%s)\n", errno, strerror(rc));
#endif
			setTaskStatus(task, TaskStatusCannotCreatePaquetThread);
			if (receiveBuffer != NULL)
				pokeBuffer(receiveBuffer);
			pokeBuffer(paquet->inputBuffer);
        	break;
        }
	} while (getTaskStatus(task) == TaskStatusGood);

	if (receiveBuffer != NULL)
		pokeBuffer(receiveBuffer);
}

int authentifyDialogue(struct task *task, struct dialogueDemande *dialogueDemande)
{
	struct dialogueVerdict dialogueVerdict;
	int rc = 0;

	struct dbh *dbh = peekDB(DB_AUTHENTICATION);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	int invalidDeviceId = 0;
	int invalidProfileId = 0;

	uint64_t deviceId;
	uint64_t profileId = 0;
	uint64_t sessionId;

	deviceId = deviceIdByToken(dbh, (char *)&dialogueDemande->deviceToken);
	if (deviceId == 0) {
#ifdef DEBUG
		fprintf(stderr, "Cannot authenticate device by token\n");
#endif
		setTaskStatus(task, TaskStatusDeviceAuthenticationFailed);

		invalidDeviceId = 1;
	}

	if (deviceId != 0) {
		int i;

		for (i = 0; i < TokenBinarySize; i++)
			if (dialogueDemande->profileToken[i] != '\0')
				break;

		// If the end of token is reached then this is not an empty token.
		// Empty profile token means that user did not create profile yet.
		// If a non-empty profile token provided then verify it.
		//
		if (i < TokenBinarySize) {
			profileId = profileIdByToken(dbh, (char *)&dialogueDemande->profileToken);
			if (profileId == 0) {
#ifdef DEBUG
				fprintf(stderr, "Cannot authenticate profile by token\n");
#endif
				setTaskStatus(task, TaskStatusProfileAuthenticationFailed);

				invalidProfileId = 1;
			}
		} else {
			bzero(&dialogueDemande->profileToken, TokenBinarySize);
		}
	}

	rc = getSessionForDevice(task, dbh, deviceId, &sessionId,
		(char *)&dialogueDemande->sessionToken,
		(char *)&dialogueVerdict.sessionToken);
	if (rc < 0) {
#ifdef DEBUG
		fprintf(stderr, "Cannot get session\n");
#endif
		setTaskStatus(task, TaskStatusCannotGetSession);
	}

	fprintf(stderr, "DEVICE:%lu PROFILE:%lu SESSION:%lu\n",
		be64toh(deviceId),
		be64toh(profileId),
		be64toh(sessionId));

	pokeDB(dbh);

	task->deviceId = deviceId;
	task->profileId = profileId;
	task->sessionId = sessionId;

	dialogueVerdict.dialogueSignature = DialogueSignature;

	if (invalidDeviceId != 0) {
		dialogueVerdict.verdictCode = DialogueVerdictInvalidDevice;
		rc = -1;
	} else if (invalidProfileId != 0) {
		dialogueVerdict.verdictCode = DialogueVerdictInvalidProfile;
		rc = -1;
	} else if (memcmp(dialogueDemande->sessionToken, dialogueVerdict.sessionToken, TokenBinarySize) != 0) {
		dialogueVerdict.verdictCode = DialogueVerdictNewSession;
		rc = 0;
	} else {
		dialogueVerdict.verdictCode = DialogueVerdictWelcome;
		rc = 0;
	}

	dialogueVerdict.dialogueSignature = htobe64(dialogueVerdict.dialogueSignature);
	dialogueVerdict.verdictCode = htobe32(dialogueVerdict.verdictCode);

	if (sendFixed(task, (char *)&dialogueVerdict, sizeof(dialogueVerdict)) != 0) {
		setTaskStatus(task, TaskStatusCannotSendDialogueVerdict);
		rc = -1;
	}

	pokeDB(dbh);

	return rc;
}

void *taskThread(void *arg)
{
	struct task *task = (struct task *)arg;
	int rc;

	struct dialogueDemande dialogueDemande;
	rc = receiveFixed(task, (char *)&dialogueDemande, sizeof(dialogueDemande));
	if (rc != 0) {
#ifdef DEBUG
       	fprintf(stderr, "No dialoge demande\n");
#endif
		setTaskStatus(task, TaskStatusMissingDialogueDemande);
	} else {
		uint32_t dialogueType = be32toh(dialogueDemande.dialogueType);
		switch (dialogueType)
		{
			case DialogueTypeAnticipant:
#ifdef DEBUG
		       	fprintf(stderr, "Start anticipant dialogue\n");
#endif
				dialogueAnticipant(task);
				break;

			case DialogueTypeRegular:
				authentifyDialogue(task, &dialogueDemande);
#ifdef DEBUG
		       	fprintf(stderr, "Start regular dialogue\n");
#endif
				dialogueRegular(task);
				break;
		}
	}

	int taskStatus = getTaskStatus(task);
#ifdef DEBUG
	if (taskStatus == TaskStatusGood)
       	fprintf(stderr, "Task complete\n");
	else
       	fprintf(stderr, "Task cancelled with status 0x%016X\n", taskStatus);
#endif

	close(task->sockFD);

	free(task->clientIP);

	free(task);

	pthread_exit(NULL);

	return NULL;
}

inline struct task *startTask(int sockFD, char *clientIP)
{
	struct task *task;
	int rc;

	task = malloc(sizeof(struct task));
	if (task == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Out of memory\n");
#endif
        return NULL;
    }

	task->status = TaskStatusGood;

	rc = pthread_spin_init(&task->statusLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
#ifdef DEBUG
		fprintf(stderr, "Can't initialize spinlock: %d (%s)\n", errno, strerror(errno));
#endif
        return NULL;
    }

	rc = pthread_spin_init(&task->heavyJobLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
#ifdef DEBUG
		fprintf(stderr, "Can't initialize spinlock: %d (%s)\n", errno, strerror(errno));
#endif
        return NULL;
    }

	rc = pthread_spin_init(&task->downloadLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
#ifdef DEBUG
		fprintf(stderr, "Can't initialize spinlock: %d (%s)\n", errno, strerror(errno));
#endif
        return NULL;
    }

	task->sockFD = sockFD;
	task->clientIP = clientIP;

    rc = pthread_create(&task->thread, NULL, &taskThread, task);
    if (rc != 0) {
#ifdef DEBUG
        fprintf(stderr, "Can't create main task: %d (%s)\n", errno, strerror(rc));
#endif
        return NULL;
    }

	return task;
}
