#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "chalkboard.h"
#include "mmps.h"
#include "paquet.h"
#include "report.h"
#include "tasks.h"
#include "task_kernel.h"
#include "task_list.h"
#include "task_xmit.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

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
	int					sockFD,
	char				*clientIP)
{
	struct MMPS_Buffer	*taskBuffer;
	struct task			*task;
	int 				rc;

#ifdef TASKS
	reportInfo("Starting new task");
#endif

	taskBuffer = MMPS_PeekBuffer(chalkboard->pools.task, BUFFER_TASK);
	if (taskBuffer == NULL) {
        reportError("Out of memory");
        return NULL;
    }

	task = (struct task *)taskBuffer->data;
	task->containerBuffer = taskBuffer;

	task->taskId = taskBuffer->bufferId;

	task->status = TaskStatusGood;

	task->xmit.sockFD = sockFD;

	strncpy(task->clientIP, clientIP, sizeof(task->clientIP));

#ifdef TASKS
	reportInfo("... creating a thread");
#endif

    rc = pthread_create(&task->thread, NULL, &taskThread, task);
    if (rc != 0) {
    	MMPS_PokeBuffer(taskBuffer);
        reportError("Cannot create task: errno=%d", errno);
        return NULL;
    }

#ifdef TASKS
	reportInfo("... created thread for task 0x%08luX to serve %s",
		(unsigned long) task,
		task->clientIP);
#endif

	return task;
}

inline void
__setTaskStatus(struct task *task, uint64 statusMask)
{
	pthread_spin_lock(&task->statusLock);
	task->status |= statusMask;
	pthread_spin_unlock(&task->statusLock);
}

inline uint64
getTaskStatus(struct task *task)
{
	pthread_spin_lock(&task->statusLock);
	uint64 status = task->status;
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
			if (paquetUnderCursor->nextInChain == paquet)
			{
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
	struct task	*task = (struct task *)arg;
	int			rc;

	rc = taskInit(task);
	if (rc != 0)
		pthread_exit(NULL);

    pthread_cleanup_push(taskCleanup, task);

	rc = receiveFixed(task, (char *)&task->dialogue.demande, sizeof(task->dialogue.demande));
	if (rc != 0) {
#ifdef TASK_THREAD
       	reportInfo("No dialoge demande");
#endif
		setTaskStatus(task, TaskStatusMissingDialogueDemande);
	} else {
		uint32 dialogueType = be32toh(task->dialogue.demande.dialogueType);
		switch (dialogueType)
		{
			case API_DialogueTypeAnticipant:
#ifdef TASK_THREAD
		       	reportInfo("Start anticipant dialogue");
#endif
				dialogueAnticipant(task);
				break;

			case API_DialogueTypeRegular:
				authentifyDialogue(task);
#ifdef TASK_THREAD
		       	reportInfo("Start regular dialogue");
#endif
				dialogueRegular(task);
				break;
		}
	}

#ifdef TASK_THREAD
	long taskStatus = getTaskStatus(task);
	if (taskStatus == TaskStatusGood) {
       	reportInfo("Task complete");
	} else {
	    int commonStatus = taskStatus & 0xFFFFFFFF;
	    int communicationStatus = taskStatus >> 32;
       	reportInfo("Task cancelled with status 0x%08X:%08X",
       	    communicationStatus, commonStatus);
    }
#endif

    pthread_cleanup_pop(1);

	pthread_exit(NULL);
}

int
taskInit(struct task *task)
{
	pthread_mutexattr_t mutexAttr;
	int					rc;

    taskListPushTask(task->taskId, task);

    pthread_mutexattr_init(&mutexAttr);
    //pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);

	rc = pthread_spin_init(&task->statusLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		reportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

    rc = pthread_mutex_init(&task->xmit.receiveMutex, &mutexAttr);
	if (rc != 0) {
		reportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    rc = pthread_mutex_init(&task->xmit.sendMutex, &mutexAttr);
	if (rc != 0) {
		reportError("Cannot initialize mutex: rc=%d", rc);
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

    rc = pthread_mutex_init(&task->paquet.downloadMutex, &mutexAttr);
	if (rc != 0) {
		reportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    rc = pthread_mutex_init(&task->broadcast.editMutex, &mutexAttr);
	if (rc != 0) {
		reportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    rc = pthread_mutex_init(&task->broadcast.waitMutex, &mutexAttr);
	if (rc != 0) {
		reportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    rc = pthread_cond_init(&task->broadcast.waitCondition, NULL);
	if (rc != 0) {
		reportError("Cannot initialize condition: rc=%d", rc);
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
	struct paquet	*paquetToCancel;
	int 			rc;

	do {
		pthread_spin_lock(&task->paquet.chainLock);

		paquetToCancel = task->paquet.chainAnchor;

		pthread_spin_unlock(&task->paquet.chainLock);

		if (paquetToCancel != NULL)
			PaquetCancel(paquetToCancel);

	} while (paquetToCancel != NULL);

	close(task->xmit.sockFD);

	rc = pthread_spin_destroy(&task->statusLock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

	rc = pthread_mutex_destroy(&task->xmit.receiveMutex);
	if (rc != 0)
		reportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_mutex_destroy(&task->xmit.sendMutex);
	if (rc != 0)
		reportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_spin_destroy(&task->paquet.chainLock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

	rc = pthread_spin_destroy(&task->paquet.heavyJobLock);
	if (rc != 0)
		reportError("Cannot destroy spinlock: errno=%d", errno);

	rc = pthread_mutex_destroy(&task->paquet.downloadMutex);
	if (rc != 0)
		reportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_mutex_destroy(&task->broadcast.editMutex);
	if (rc != 0)
		reportError("Cannot destroy mutex: rc=%d", rc);

	rc = pthread_mutex_destroy(&task->broadcast.waitMutex);
	if (rc != 0)
		reportError("Cannot destroy mutex: rc=%d", rc);

    rc = pthread_cond_destroy(&task->broadcast.waitCondition);
	if (rc != 0)
		reportError("Cannot destroy condition: rc=%d", rc);

    taskListPushTask(task->taskId, NULL);

    MMPS_PokeBuffer(task->containerBuffer);
}
