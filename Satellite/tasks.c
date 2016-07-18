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
TaskThread(void *arg);

int
TaskInit(struct Task *task);

void
TaskCleanup(void *arg);

struct Task *
StartTask(
	int					sockFD,
	char				*clientIP)
{
	struct MMPS_Buffer	*taskBuffer;
	struct Task			*task;
	int 				rc;

#ifdef TASKS
	ReportInfo("Starting new task");
#endif

	taskBuffer = MMPS_PeekBuffer(chalkboard->pools.task, BUFFER_TASK);
	if (taskBuffer == NULL) {
        ReportSoftAlert("Out of memory");
        return NULL;
    }

	task = (struct Task *)taskBuffer->data;
	task->containerBuffer = taskBuffer;

	task->taskId = taskBuffer->bufferId;

	task->status = TaskStatusGood;

	task->xmit.sockFD = sockFD;

	strncpy(task->clientIP, clientIP, sizeof(task->clientIP));

#ifdef TASKS
	ReportInfo("... creating a thread");
#endif

    rc = pthread_create(&task->thread, NULL, &TaskThread, task);
    if (rc != 0) {
    	MMPS_PokeBuffer(taskBuffer);
        ReportError("Cannot create task: errno=%d", errno);
        return NULL;
    }

#ifdef TASKS
	ReportInfo("... created thread for task 0x%08luX to serve %s",
		(unsigned long) task,
		task->clientIP);
#endif

	return task;
}

inline void
__SetTaskStatus(struct Task *task, uint64 statusMask)
{
	pthread_spin_lock(&task->statusLock);
	task->status |= statusMask;
	pthread_spin_unlock(&task->statusLock);
}

inline uint64
GetTaskStatus(struct Task *task)
{
	pthread_spin_lock(&task->statusLock);
	uint64 status = task->status;
	pthread_spin_unlock(&task->statusLock);
	return status;
}

void
AppentPaquetToTask(struct Task *task, struct Paquet *paquet)
{
	struct Paquet *paquetUnderCursor;

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
RemovePaquetFromTask(struct Task *task, struct Paquet *paquet)
{
	struct Paquet *paquetUnderCursor;

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
TaskThread(void *arg)
{
	struct Task	*task = (struct Task *)arg;
	int			rc;

	rc = TaskInit(task);
	if (rc != 0)
		pthread_exit(NULL);

    pthread_cleanup_push(TaskCleanup, task);

	rc = ReceiveFixed(task, (char *)&task->dialogue.demande, sizeof(task->dialogue.demande));
	if (rc != 0) {
#ifdef TASK_THREAD
       	ReportInfo("No dialoge demande");
#endif
		SetTaskStatus(task, TaskStatusMissingDialogueDemande);
	} else {
		uint32 dialogueType = be32toh(task->dialogue.demande.dialogueType);
		switch (dialogueType)
		{
			case API_DialogueTypeAnticipant:
#ifdef TASK_THREAD
		       	ReportInfo("Start anticipant dialogue");
#endif
				DialogueAnticipant(task);
				break;

			case API_DialogueTypeRegular:
				AuthentifyDialogue(task);
#ifdef TASK_THREAD
		       	ReportInfo("Start regular dialogue");
#endif
				DialogueRegular(task);
				break;
		}
	}

#ifdef TASK_THREAD
	long taskStatus = GetTaskStatus(task);
	if (taskStatus == TaskStatusGood) {
       	ReportInfo("Task complete");
	} else {
	    int commonStatus = taskStatus & 0xFFFFFFFF;
	    int communicationStatus = taskStatus >> 32;
       	ReportInfo("Task cancelled with status 0x%08X:%08X",
       	    communicationStatus, commonStatus);
    }
#endif

    pthread_cleanup_pop(1);

	pthread_exit(NULL);
}

int
TaskInit(struct Task *task)
{
	pthread_mutexattr_t mutexAttr;
	int					rc;

    TaskListPushTask(task->taskId, task);

    pthread_mutexattr_init(&mutexAttr);
    //pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);

	rc = pthread_spin_init(&task->statusLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		ReportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

#ifdef DUPLEX
    rc = pthread_mutex_init(&task->xmit.receiveMutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    rc = pthread_mutex_init(&task->xmit.sendMutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }
#else
    rc = pthread_mutex_init(&task->xmit.xmitMutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }
#endif

	rc = pthread_spin_init(&task->paquet.chainLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		ReportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

	rc = pthread_spin_init(&task->paquet.heavyJobLock, PTHREAD_PROCESS_PRIVATE);
	if (rc != 0) {
		ReportError("Cannot initialize spinlock: errno=%d", errno);
        return -1;
    }

    rc = pthread_mutex_init(&task->paquet.downloadMutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    rc = pthread_mutex_init(&task->broadcast.editMutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    rc = pthread_mutex_init(&task->broadcast.waitMutex, &mutexAttr);
	if (rc != 0) {
		ReportError("Cannot initialize mutex: rc=%d", rc);
        return -1;
    }

    rc = pthread_cond_init(&task->broadcast.waitCondition, NULL);
	if (rc != 0) {
		ReportError("Cannot initialize condition: rc=%d", rc);
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
TaskCleanup(void *arg)
{
	struct Task 	*task = (struct Task *)arg;
	struct Paquet	*paquetToCancel;
	int 			rc;

	do {
		pthread_spin_lock(&task->paquet.chainLock);

		paquetToCancel = task->paquet.chainAnchor;

		pthread_spin_unlock(&task->paquet.chainLock);

		if (paquetToCancel != NULL)
			PaquetCancel(paquetToCancel);

	} while (paquetToCancel != NULL);

	close(task->xmit.sockFD);

	pthread_spin_lock(&task->statusLock);
	pthread_spin_unlock(&task->statusLock);
	rc = pthread_spin_destroy(&task->statusLock);
	if (rc != 0)
		ReportError("Cannot destroy spinlock: errno=%d", errno);

#ifdef DUPLEX
	pthread_mutex_lock(&task->xmit.receiveMutex);
	pthread_mutex_unlock(&task->xmit.receiveMutex);
	rc = pthread_mutex_destroy(&task->xmit.receiveMutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	pthread_mutex_lock(&task->xmit.sendMutex);
	pthread_mutex_unlock(&task->xmit.sendMutex);
	rc = pthread_mutex_destroy(&task->xmit.sendMutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);
#else
	pthread_mutex_lock(&task->xmit.xmitMutex);
	pthread_mutex_unlock(&task->xmit.xmitMutex);
	rc = pthread_mutex_destroy(&task->xmit.xmitMutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);
#endif

	pthread_mutex_lock(&task->paquet.chainLock);
	pthread_mutex_unlock(&task->paquet.chainLock);
	rc = pthread_spin_destroy(&task->paquet.chainLock);
	if (rc != 0)
		ReportError("Cannot destroy spinlock: errno=%d", errno);

	pthread_mutex_lock(&task->paquet.heavyJobLock);
	pthread_mutex_unlock(&task->paquet.heavyJobLock);
	rc = pthread_spin_destroy(&task->paquet.heavyJobLock);
	if (rc != 0)
		ReportError("Cannot destroy spinlock: errno=%d", errno);

	pthread_mutex_lock(&task->paquet.downloadMutex);
	pthread_mutex_unlock(&task->paquet.downloadMutex);
	rc = pthread_mutex_destroy(&task->paquet.downloadMutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	pthread_mutex_lock(&task->broadcast.editMutex);
	pthread_mutex_unlock(&task->broadcast.editMutex);
	rc = pthread_mutex_destroy(&task->broadcast.editMutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

	pthread_mutex_lock(&task->broadcast.waitMutex);
	pthread_mutex_unlock(&task->broadcast.waitMutex);
	rc = pthread_mutex_destroy(&task->broadcast.waitMutex);
	if (rc != 0)
		ReportError("Cannot destroy mutex: rc=%d", rc);

    rc = pthread_cond_destroy(&task->broadcast.waitCondition);
	if (rc != 0)
		ReportError("Cannot destroy condition: rc=%d", rc);

    TaskListPushTask(task->taskId, NULL);

    MMPS_PokeBuffer(task->containerBuffer);
}
