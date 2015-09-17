#ifndef _TASKS_
#define _TASKS_

#include <semaphore.h>
#include "buffers.h"

typedef struct task {
	int					taskId;
	sem_t				waitForJob;
	pthread_t			thread;
	int					sockFD;
	char				*clientIP;
	struct buffer		*request;
	struct buffer		*response;
} task_t;

typedef struct receiver {
	int					receiverId;
	sem_t				waitForReadyToGo;
	sem_t				waitForComplete;
	pthread_t			thread;
	task_t				*task;
} receiver_t;

typedef struct transmitter {
	int					transmitterId;
	sem_t				waitForReadyToGo;
	sem_t				waitForComplete;
	pthread_t			thread;
	task_t				*task;
} transmitter_t;

void *taskThread(void *arg);
void *receiverThread(void *arg);
void *transmitterThread(void *arg);

#endif
