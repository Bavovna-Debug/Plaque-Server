#include <c.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "anticipant.h"
#include "api.h"
#include "buffers.h"
#include "desk.h"
#include "paquet.h"
#include "plaques_edit.h"
#include "plaques_query.h"
#include "profiles.h"
#include "report.h"
#include "session.h"
#include "tasks.h"
#include "task_kernel.h"
#include "task_xmit.h"

#ifdef ANTICIPANT_ALL
#define ANTICIPANT_DIALOGUE
#define ANTICIPANT_DIALOGUE_REGULAR
#define ANTICIPANT_DIALOGUE_AUTH
#endif

void
dialogueAnticipant(struct task *task)
{
	int rc;

#ifdef ANTICIPANT_DIALOGUE
        reportLog("Anticipant begin");
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

#ifdef ANTICIPANT_DIALOGUE
        reportLog("Anticipant end");
#endif
}

void
dialogueRegular(struct task *task)
{
	int rc;

	struct buffer *receiveBuffer = NULL;

	rc = setSessionOnline(task);
	if (rc < 0) {
#ifdef ANTICIPANT_DIALOGUE_REGULAR
		reportError("Cannot set session online");
#endif
		setTaskStatus(task, TaskStatusCannotSetSessionOnline);
	}

	do {
#ifdef ANTICIPANT_DIALOGUE_REGULAR
        reportLog("Dialoque loop");
#endif
    	struct buffer *buffer;
        struct paquet *paquet;

		// Get a buffer for new paquet.
		//
    	buffer = peekBuffer(task->desk->pools.paquet);
    	if (buffer == NULL) {
            reportError("Out of memory");
			setTaskStatus(task, TaskStatusOutOfMemory);
			break;
        }

	    paquet = (struct paquet *)buffer->data;
    	paquet->containerBuffer = buffer;

		paquet->task = task;
		paquet->nextInChain = NULL;
		paquet->inputBuffer = NULL;
		paquet->outputBuffer = NULL;

		int receiveNeeded;

		// If there is no rest data from previous receive then allocate a new buffer and start receive.
		//
		if (receiveBuffer == NULL) {
			receiveBuffer = peekBufferOfSize(task->desk->pools.dynamic, KB);

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
				uint32 payloadSize = be32toh(pilot->payloadSize);

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

#ifdef ANTICIPANT_DIALOGUE_REGULAR
		struct paquetPilot *pilot = (struct paquetPilot *)receiveBuffer->data;
		reportLog("Received paquet  %u with command 0x%08X with %d bytes (payload %d bytes)",
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
#ifdef ANTICIPANT_DIALOGUE_REGULAR
			reportLog("Received data incomplete");
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
			receiveBuffer = peekBufferOfSize(task->desk->pools.dynamic, KB);
			if (receiveBuffer == NULL) {
				setTaskStatus(task, TaskStatusOutOfMemory);
				break;
			}

			// Move the rest of data from current paquet to a new receive buffer.
			//
			int sizeOfRestOfData = sliceDataBuffer->dataSize - paquetDataRest;
			memcpy(receiveBuffer->data,
					sliceDataBuffer->data + paquetDataRest,
					sizeOfRestOfData);
			receiveBuffer->dataSize = sizeOfRestOfData;

			// Cut the rest of data from current paquet.
			//
			sliceDataBuffer->dataSize = paquetDataRest;
		}

		// Start new thread to process current paquet.
		//
	    rc = pthread_create(&paquet->thread, NULL, &paquetThread, paquet);
    	if (rc != 0) {
#ifdef ANTICIPANT_DIALOGUE_REGULAR
        	reportError("Cannot create paquet thread: errno=%d", errno);
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

	rc = setSessionOffline(task);
	if (rc < 0) {
#ifdef ANTICIPANT_DIALOGUE_REGULAR
		reportError("Cannot set session offline");
#endif
		setTaskStatus(task, TaskStatusCannotSetSessionOffline);
	}
}

int
authentifyDialogue(struct task *task, struct dialogueDemande *dialogueDemande)
{
	struct dialogueVerdict dialogueVerdict;
	int rc = 0;

	struct dbh *dbh = peekDB(task->desk->dbh.auth);
	if (dbh == NULL) {
		setTaskStatus(task, TaskStatusNoDatabaseHandlers);
		return -1;
	}

	int invalidDeviceId = 0;
	int invalidProfileId = 0;

	uint64 deviceId;
	uint64 profileId = 0;
	uint64 sessionId;

	deviceId = deviceIdByToken(dbh, (char *)&dialogueDemande->deviceToken);
	if (deviceId == 0) {
		reportLog("Cannot authenticate device by token");

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
				reportLog("Cannot authenticate profile by token");

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
		reportLog("Cannot get session");
		setTaskStatus(task, TaskStatusCannotGetSession);
	}

#ifdef ANTICIPANT_DIALOGUE_AUTH
	reportLog("Dialogue authentified: deviceId=%lu profileId=%lu sessionId=%lu",
		be64toh(deviceId),
		be64toh(profileId),
		be64toh(sessionId));
#endif

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

	return rc;
}
