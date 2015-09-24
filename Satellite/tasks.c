#include <errno.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "buffers.h"
#include "desk.h"
#include "paquet.h"
#include "report.h"
#include "tasks.h"
#include "task_kernel.h"
#include "task_xmit.h"

// TASKS
// TASK_THREAD

void *
taskThread(void *arg);

int
taskInit(struct task *task);

void
taskCleanup(void *arg);

struct task *
startTask(
	struct desk		*desk,
	int				sockFD,
	char			*clientIP)
{
	struct buffer	*buffer;
	struct task		*task;
	int rc;

#ifdef TASKS
	reportLog("Starting new task");
#endif

	buffer = peekBuffer(desk->pools.task);
	if (buffer == NULL) {
        reportError("Out of memory");
        return NULL;
    }

	task = (struct task *)buffer->data;
	task->containerBuffer = buffer;

	task->taskId = buffer->bufferId;

	task->desk = desk;

	task->status = TaskStatusGood;

	task->xmit.sockFD = sockFD;

	strncpy(task->clientIP, clientIP, sizeof(task->clientIP));

#ifdef TASKS
	reportLog("... creating a thread");
#endif

    rc = pthread_create(&task->thread, NULL, &taskThread, task);
    if (rc != 0) {
    	pokeBuffer(buffer);
        reportError("Cannot create main task: errno=%d", errno);
        return NULL;
    }

#ifdef TASKS
	reportLog("... created thread for task 0x%08X to serve %s", task, task->clientIP);
#endif

	return task;
}

inline void
__setTaskStatus(struct task *task, long statusMask)
{
	pthread_spin_lock(&task->statusLock);
	task->status |= statusMask;
	pthread_spin_unlock(&task->statusLock);
}

inline long
getTaskStatus(struct task *task)
{
	pthread_spin_lock(&task->statusLock);
	long status = task->status;
	pthread_spin_unlock(&task->statusLock);
	return status;
}

void
appentPaquetToTask(struct task *task, struct paquet *paquet)
{
	struct paquet *paquetUnderCursor;

	pthread_spin_lock(&task->paquet.chainLock);

	if (task->paquet.chainAnchor == NULL) {
		task->paquet.chainAnchor = paquet;
	} else {
		paquetUnderCursor = task->paquet.chainAnchor;

		while (paquetUnderCursor->nextInChain != NULL)
			paquetUnderCursor = paquetUnderCursor->nextInChain;

		paquetUnderCursor->nextInChain = paquet;
	}

	pthread_spin_unlock(&task->paquet.chainLock);
}

void
removePaquetFromTask(struct task *task, struct paquet *paquet)
{
	struct paquet *paquetUnderCursor;

	pthread_spin_lock(&task->paquet.chainLock);

	if (task->paquet.chainAnchor == paquet) {
		task->paquet.chainAnchor = task->paquet.chainAnchor->nextInChain;
	} else {
		paquetUnderCursor = task->paquet.chainAnchor;

		do {
			if (paquetUnderCursor->nextInChain == paquet) {
				paquetUnderCursor->nextInChain = paquetUnderCursor->nextInChain->nextInChain;
				break;
			}

			paquetUnderCursor = paquetUnderCursor->nextInChain;
		} while (paquetUnderCursor != NULL);
	}

	pthread_spin_unlock(&task->paquet.chainLock);
}

void *
taskThread(void *arg)
{
	struct task *task = (struct task *)arg;
	struct dialogueDemande dialogueDemande;
	int rc;

    pthread_cleanup_push(taskCleanup, task);

	rc = taskInit(task);
	if (rc != 0)
		pthread_exit(NULL);

	rc = receiveFixed(task, (char *)&dialogueDemande, sizeof(dialogueDemande));
	if (rc != 0) {
#ifdef TASK_THREAD
       	reportLog("No dialoge demande");
#endif
		setTaskStatus(task, TaskStatusMissingDialogueDemande);
	} else {
		uint32 dialogueType = be32toh(dialogueDemande.dialogueType);
		switch (dialogueType)
		{
			case DialogueTypeAnticipant:
#ifdef TASK_THREAD
		       	reportLog("Start anticipant dialogue");
#endif
				dialogueAnticipant(task);
				break;

			case DialogueTypeRegular:
				authentifyDialogue(task, &dialogueDemande);
#ifdef TASK_THREAD
		       	reportLog("Start regular dialogue");
#endif
				dialogueRegular(task);
				break;
		}
	}

#ifdef TASK_THREAD
	long taskStatus = getTaskStatus(task);
	if (taskStatus == TaskStatusGood) {
       	reportLog("Task complete");
	} else {
	    int commonStatus = taskStatus & 0xFFFFFFFF;
	    int communicationStatus = taskStatus >> 32;
       	reportLog("Task cancelled with status 0x%08X:%08X",
       	    communicationStatus, commonStatus);
    }
#endif

    pthread_cleanup_pop(1);

	pthread_exit(NULL);
}

int
taskInit(struct task *task)
{
	int rc;

    taskListPushTask(task->desk, task->taskId, task);

	rc = pthread_spin_init(&task->statusLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

	rc = pthread_spin_init(&task->xmit.receiveLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

	rc = pthread_spin_init(&task->xmit.sendLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

	rc = pthread_spin_init(&task->paquet.chainLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

	rc = pthread_spin_init(&task->paquet.heavyJobLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

	rc = pthread_spin_init(&task->paquet.downloadLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

	rc = pthread_spin_init(&task->broadcast.lock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

    rc = sem_init(&task->broadcast.waitForBroadcast, 0, 0);
	if (rc != 0) {
        reportError("Cannot initialize semaphore: errno=%d", errno);
        return -1;
    }

	task->broadcast.lastKnownRevision.onRadar = 0;
	task->broadcast.lastKnownRevision.inSight = 0;
	task->broadcast.lastKnownRevision.onMap = 0;

	task->broadcast.currentRevision.onRadar = 0;
	task->broadcast.currentRevision.inSight = 0;
	task->broadcast.currentRevision.onMap = 0;

	task->broadcast.broadcastPaquet = NULL;

    return 0;
}

void
taskCleanup(void *arg)
{
	struct task 	*task = (struct task *)arg;
	int 			rc;

	while (task->paquet.chainAnchor != NULL)
		paquetCancel(task->paquet.chainAnchor);

	close(task->xmit.sockFD);

	rc = pthread_spin_destroy(&task->statusLock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

	rc = pthread_spin_destroy(&task->xmit.receiveLock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

	rc = pthread_spin_destroy(&task->xmit.sendLock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

	rc = pthread_spin_destroy(&task->paquet.chainLock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

	rc = pthread_spin_destroy(&task->paquet.heavyJobLock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

	rc = pthread_spin_destroy(&task->paquet.downloadLock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

	rc = pthread_spin_destroy(&task->broadcast.lock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

    rc = sem_init(&task->broadcast.waitForBroadcast, 0, 0);
    if (rc != 0)
        reportError("Cannot destroy semaphore: errno=%d", errno);

    taskListPushTask(task->desk, task->taskId, NULL);

    pokeBuffer(task->containerBuffer);
}
