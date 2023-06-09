#include <c.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "anticipant.h"
#include "api.h"
#include "chalkboard.h"
#include "mmps.h"
#include "paquet.h"
#include "plaques_edit.h"
#include "plaques_query.h"
#include "profiles.h"
#include "report.h"
#include "session.h"
#include "tasks.h"
#include "task_kernel.h"
#include "task_xmit.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

int
AuthentifyDialogue(struct Task *task)
{
    int rc = 0;

    ReportInfo("[TaskKernel] Authentify dialogue");

    struct dbh *dbh = DB_PeekHandle(chalkboard->db.auth);
    if (dbh == NULL)
    {
        SetTaskStatus(task, TaskStatusNoDatabaseHandlers);
        return -1;
    }

    int invalidDeviceId = 0;
    int invalidProfileId = 0;

    uint64 deviceId;
    uint64 profileId = 0;
    uint64 sessionId;

    deviceId = DeviceIdByToken(dbh, (char *) &task->dialogue.demande.deviceToken);
    if (deviceId == 0)
    {
        ReportError("[TaskKernel] Cannot authenticate device by token");

        SetTaskStatus(task, TaskStatusDeviceAuthenticationFailed);

        invalidDeviceId = 1;
    }

    if (deviceId != 0)
    {
        int i;

        for (i = 0; i < API_TokenBinarySize; i++)
            if (task->dialogue.demande.profileToken[i] != '\0')
                break;

        // If the end of token is reached then this is not an empty token.
        // Empty profile token means that user did not create profile yet.
        // If a non-empty profile token provided then verify it.
        //
        if (i < API_TokenBinarySize)
        {
            profileId = ProfileIdByToken(dbh, (char *)&task->dialogue.demande.profileToken);
            if (profileId == 0)
            {
                ReportError("[TaskKernel] Cannot authenticate profile by token");

                SetTaskStatus(task, TaskStatusProfileAuthenticationFailed);

                invalidProfileId = 1;
            }
        }
        else
        {
            bzero(&task->dialogue.demande.profileToken, API_TokenBinarySize);
        }
    }

    rc = GetSessionForDevice(task, dbh, deviceId, &sessionId,
            (char *) &task->dialogue.demande.sessionToken,
            (char *) &task->dialogue.verdict.sessionToken);
    if (rc < 0)
    {
        ReportError("[TaskKernel] Cannot get session");
        SetTaskStatus(task, TaskStatusCannotGetSession);
    }

    ReportInfo("[TaskKernel] Dialogue authentified: deviceId=%lu profileId=%lu sessionId=%lu",
            be64toh(deviceId),
            be64toh(profileId),
            be64toh(sessionId));

    DB_PokeHandle(dbh);

    task->deviceId = deviceId;
    task->profileId = profileId;
    task->sessionId = sessionId;

    task->dialogue.verdict.dialogueSignature = API_DialogueSignature;

    if (invalidDeviceId != 0)
    {
        task->dialogue.verdict.verdictCode = API_DialogueVerdictInvalidDevice;
        rc = -1;
    }
    else if (invalidProfileId != 0)
    {
        task->dialogue.verdict.verdictCode = API_DialogueVerdictInvalidProfile;
        rc = -1;
    }
    else if (memcmp(task->dialogue.demande.sessionToken, task->dialogue.verdict.sessionToken, API_TokenBinarySize) != 0)
    {
        task->dialogue.verdict.verdictCode = API_DialogueVerdictNewSession;
        rc = 0;
    }
    else
    {
        task->dialogue.verdict.verdictCode = API_DialogueVerdictWelcome;
        rc = 0;
    }

    task->dialogue.verdict.dialogueSignature = htobe64(task->dialogue.verdict.dialogueSignature);
    task->dialogue.verdict.verdictCode = htobe32(task->dialogue.verdict.verdictCode);

    rc = SendFixed(task, (char *)&task->dialogue.verdict, sizeof(task->dialogue.verdict));
    if (rc != 0)
    {
        SetTaskStatus(task, TaskStatusCannotSendDialogueVerdict);
        rc = -1;
    }

    return rc;
}

void
DialogueAnticipant(struct Task *task)
{
    int rc;

    ReportInfo("[TaskKernel] Anticipant dialogue begin");

    struct DialogueAnticipant anticipant;
    rc = ReceiveFixed(task, (char *) &anticipant, sizeof(anticipant));
    if (rc != 0)
    {
        SetTaskStatus(task, TaskStatusMissingAnticipantRecord);
        return;
    }

    char deviceToken[API_TokenBinarySize];
    rc = RegisterDevice(task, &anticipant, (char *) &deviceToken);
    if (rc != 0)
        return;

    rc = SendFixed(task, (char *) &deviceToken, sizeof(deviceToken));
    if (rc != 0)
        return;

    ReportInfo("[TaskKernel] Anticipant dialogue end");
}

void
DialogueRegular(struct Task *task)
{
    int rc;

    struct MMPS_Buffer *receiveBuffer = NULL;

    ReportInfo("[TaskKernel] Regular dialogue begin");

    rc = SetSessionOnline(task);
    if (rc < 0)
    {
        ReportError("[TaskKernel] Cannot set session online");

        SetTaskStatus(task, TaskStatusCannotSetSessionOnline);
    }

    struct Paquet *paquet = NULL;
    do
    {
        ReportInfo("[TaskKernel] Dialoque loop");

        struct MMPS_Buffer *paquetBuffer;

        // Get a buffer for new paquet.
        //
        paquetBuffer = MMPS_PeekBuffer(chalkboard->pools.paquet, BUFFER_DIALOGUE_PAQUET);
        if (paquetBuffer == NULL)
        {
            ReportSoftAlert("[TaskKernel] Out of memory");
            SetTaskStatus(task, TaskStatusOutOfMemory);
            break;
        }

        paquet = (struct Paquet *) paquetBuffer->data;
        paquet->containerBuffer = paquetBuffer;

        paquet->task = task;
        paquet->nextInChain = NULL;
        paquet->inputBuffer = NULL;
        paquet->outputBuffer = NULL;
        paquet->pilot = paquetBuffer->follower;

        int receiveNeeded;

        // If there is no rest data from previous receive then allocate a new buffer and start receive.
        //
        if (receiveBuffer == NULL)
        {
            receiveBuffer = MMPS_PeekBufferOfSize(chalkboard->pools.dynamic,
                KB,
                BUFFER_DIALOGUE_FIRST);

            if (receiveBuffer == NULL)
            {
                SetTaskStatus(task, TaskStatusOutOfMemory);
                break;
            }

            receiveNeeded = 1;
        }
        else
        {
            // If there is rest of data from previous receive available then check
            // whether a complete paquet content is already available.

            struct PaquetPilot *pilot = (struct PaquetPilot *) paquet->pilot;
            uint32 payloadSize = be32toh(pilot->payloadSize);

            // Receive is needed if rest of data contains only a part of paquet.
            //
            if (MMPS_TotalDataSize(receiveBuffer) < payloadSize)
            {
                receiveNeeded = 1;
            }
            else
            {
                //
                // Otherwise complete paquet is available already from previous receive.
                // In such case receive should be skipped.
                //
                receiveNeeded = 0;
                FillPaquetWithPilotData(paquet);
            }
        }

        if (receiveNeeded != 0)
        {
            rc = ReceivePaquet(paquet, receiveBuffer);
            if (rc != 0)
                break;
        }

        int totalReceivedData = MMPS_TotalDataSize(receiveBuffer);

#ifdef ANTICIPANT_DIALOGUE_REGULAR
        ReportInfo("[TaskKernel] Received paquet  %u with command 0x%08X with %d bytes (payload %d bytes)",
                paquet->paquetId,
                paquet->commandCode,
                totalReceivedData,
                paquet->payloadSize);
#endif

        if (totalReceivedData < paquet->payloadSize)
        {
            //
            // If amount of data in receive buffer is less then necessary for current paquet
            // then quit with error.
            //
            ReportInfo("[TaskKernel] Received data incomplete");

            SetTaskStatus(task, TaskStatusReceivedDataIncomplete);
            MMPS_PokeBuffer(receiveBuffer);
            receiveBuffer = NULL;
            break;
        }
        else if (totalReceivedData == paquet->payloadSize)
        {
            ReportInfo("[TaskKernel] Received complete paquet");

            // If amount of data in receive buffer is exactly what is necessary for current paquet
            // associate receive buffer to paquet and reset pointer to receive buffer so
            // that a new buffer will be allocated on next loop.
            //
            paquet->inputBuffer = receiveBuffer;
            receiveBuffer = NULL;
        }
        else
        {
            ReportInfo("[TaskKernel] Received part of paquet");

            // If amount of received data is greater than necessary for current paquet
            // then cut off the rest of data from current paquet and move if
            // to a separate buffer, that would be used on next loop.

            // First associate receive buffer to current paquet.
            //
            paquet->inputBuffer = receiveBuffer;

            // Then truncate the buffer.
            //
            struct MMPS_Buffer *sliceDataBuffer = receiveBuffer;
            int paquetDataRest = paquet->payloadSize;
            while (sliceDataBuffer->next != NULL)
            {
                paquetDataRest -= sliceDataBuffer->dataSize;
                sliceDataBuffer = sliceDataBuffer->next;
            }

            // Allocate new receive buffer to be used on next loop.
            //
            receiveBuffer = MMPS_PeekBufferOfSize(
                    chalkboard->pools.dynamic,
                    KB,
                    BUFFER_DIALOGUE_FOLLOWING);
            if (receiveBuffer == NULL)
            {
                SetTaskStatus(task, TaskStatusOutOfMemory);
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
        rc = pthread_create(&paquet->thread, NULL, &PaquetThread, paquet);
        if (rc == 0)
        {
            paquet = NULL;
        }
        else
        {
            ReportError("[TaskKernel] Cannot create paquet thread: errno=%d", errno);

            SetTaskStatus(task, TaskStatusCannotCreatePaquetThread);
/*
            if (receiveBuffer != NULL)
                MMPS_PokeBuffer(receiveBuffer);
            MMPS_PokeBuffer(paquet->inputBuffer);
*/
            break;
        }
    }
    while (GetTaskStatus(task) == TaskStatusGood);

    // Release ressources of paquet in case something went wrong.
    //
    if (paquet != NULL)
    {
        if (paquet->inputBuffer != NULL)
            MMPS_PokeBuffer(paquet->inputBuffer);

        MMPS_PokeBuffer(paquet->containerBuffer);
    }

    // Release ressources of temporary receive buffer in case something went wrong.
    //
    if (receiveBuffer != NULL)
        MMPS_PokeBuffer(receiveBuffer);

    rc = SetSessionOffline(task);
    if (rc < 0)
    {

        ReportError("[TaskKernel] Cannot set session offline");

        SetTaskStatus(task, TaskStatusCannotSetSessionOffline);
    }

    ReportInfo("[TaskKernel] Regular dialogue end");
}
