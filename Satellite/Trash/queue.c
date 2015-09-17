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
#include "buffers.h"
#include "queue.h"
#include "tasks.h"
#include "processor.h"

#define RESET_SEMAPHORES_IF_NECCESSARY

#define NUMBER_OF_TASKS			1000
#define NUMBER_OF_RECEIVERS		1000
#define NUMBER_OF_TRANSMITTERS	1000

#define NOTHING				-1

typedef struct chain {
	pthread_spinlock_t	lock;
	int					numberOfThreads;
	int					peekCursor;
	int					pokeCursor;
	void				*block;
	int					ids[];
} chain_t;

static struct chain			*taskChain;
static void					*tasksBlock;
static struct task			*tasks[NUMBER_OF_TASKS];

static struct chain			*receiverChain;
static void					*receiversBlock;
static struct receiver		*receivers[NUMBER_OF_RECEIVERS];

static struct chain			*transmitterChain;
static void					*transmittersBlock;
static struct transmitter	*transmitters[NUMBER_OF_TRANSMITTERS];

void prepareTaskChain(int numberOfTasks)
{
	taskChain = malloc(sizeof(struct chain) + numberOfTasks * sizeof(int));
	if (taskChain == NULL) {
        fprintf(stderr, "No memory\n");
        return;
    }

	taskChain->block = malloc(numberOfTasks * sizeof(struct task));
	if (taskChain->block == NULL) {
		fprintf(stderr, "No memory\n");
		return;
	}

	pthread_spin_init(&taskChain->lock, PTHREAD_PROCESS_PRIVATE);

	taskChain->numberOfThreads = numberOfTasks;
	taskChain->peekCursor = 0;
	taskChain->pokeCursor = 0;

	int taskId;
	for (taskId = 0; taskId < taskChain->numberOfThreads; taskId++)
	{
		struct task *task = taskChain->block + taskId * sizeof(struct task);

		task->taskId = taskId;

		if (sem_init(&task->waitForJob, 0, 0) == -1)
			fprintf(stderr, "Can't initialize semaphore: %d %s\n", errno, strerror(errno));

        int rc = pthread_create(&task->thread, NULL, &taskThread, task);
        if (rc != 0)
            fprintf(stderr, "Can't create main task: %d %s\n", errno, strerror(rc));

		task->request = NULL;
		task->response = NULL;
	}

	for (taskId = 0; taskId < taskChain->numberOfThreads; taskId++)
		taskChain->ids[taskId] = taskId;
}

void prepareReceiverChain(int numberOfReceivers)
{
	receiverChain = malloc(sizeof(struct chain) + numberOfReceivers * sizeof(int));
	if (receiverChain == NULL) {
        fprintf(stderr, "No memory\n");
        return;
    }

	receiverChain->block = malloc(numberOfReceivers * sizeof(struct receiver));
	if (receiverChain->block == NULL) {
		fprintf(stderr, "No memory\n");
		return;
	}

	pthread_spin_init(&receiverChain->lock, PTHREAD_PROCESS_PRIVATE);

	receiverChain->numberOfThreads = numberOfReceivers;
	receiverChain->peekCursor = 0;
	receiverChain->pokeCursor = 0;

	int receiverId;
	for (receiverId = 0; receiverId < receiverChain->numberOfThreads; receiverId++)
	{
		struct receiver *receiver = receiverChain->block + receiverId * sizeof(struct receiver);

		receiver->receiverId = receiverId;

		if (sem_init(&receiver->waitForReadyToGo, 0, 0) == -1)
			fprintf(stderr, "Can't initialize semaphore: %d %s\n", errno, strerror(errno));

		if (sem_init(&receiver->waitForComplete, 0, 0) == -1)
			fprintf(stderr, "Can't initialize semaphore: %d %s\n", errno, strerror(errno));

		int rc = pthread_create(&receiver->thread, NULL, &receiverThread, receiver);
		if (rc != 0)
            fprintf(stderr, "Can't create thread: %d %s\n", errno, strerror(rc));
	}

	for (receiverId = 0; receiverId < receiverChain->numberOfThreads; receiverId++)
		receiverChain->ids[receiverId] = receiverId;
}

void prepareTransmitterChain(int numberOfTransmitters)
{
	transmitterChain = malloc(sizeof(struct chain) + numberOfTransmitters * sizeof(int));
	if (transmitterChain == NULL) {
        fprintf(stderr, "No memory\n");
        return;
    }

	transmitterChain->block = malloc(numberOfTransmitters * sizeof(struct transmitter));
	if (transmitterChain->block == NULL) {
		fprintf(stderr, "No memory\n");
		return;
	}

	pthread_spin_init(&transmitterChain->lock, PTHREAD_PROCESS_PRIVATE);

	transmitterChain->numberOfThreads = numberOfTransmitters;
	transmitterChain->peekCursor = 0;
	transmitterChain->pokeCursor = 0;

	int transmitterId;
	for (transmitterId = 0; transmitterId < transmitterChain->numberOfThreads; transmitterId++)
	{
		struct transmitter *transmitter = transmitterChain->block + transmitterId * sizeof(struct transmitter);

		transmitter->transmitterId = transmitterId;

		if (sem_init(&transmitter->waitForReadyToGo, 0, 0) == -1)
			fprintf(stderr, "Can't initialize semaphore: %d %s\n", errno, strerror(errno));

		if (sem_init(&transmitter->waitForComplete, 0, 0) == -1)
			fprintf(stderr, "Can't initialize semaphore: %d %s\n", errno, strerror(errno));

		int rc = pthread_create(&transmitter->thread, NULL, &transmitterThread, transmitter);
		if (rc != 0)
            fprintf(stderr, "Can't create thread: %d %s\n", errno, strerror(rc));
	}

	for (transmitterId = 0; transmitterId < transmitterChain->numberOfThreads; transmitterId++)
		transmitterChain->ids[transmitterId] = transmitterId;
}

void constructQueue(void)
{
	prepareTaskChain(NUMBER_OF_TASKS);
	prepareReceiverChain(NUMBER_OF_RECEIVERS);
	prepareTransmitterChain(NUMBER_OF_TRANSMITTERS);

	//sleep(1);
}

void destructQueue(void)
{
}

struct task *peekTask(int sockFD, char *clientIP)
{
	struct chain *chain = taskChain;
	struct task *task;

	pthread_spin_lock(&chain->lock);

	int taskId = chain->ids[chain->peekCursor];
	if (taskId == NOTHING) {
		task = NULL;
	} else {
		task = chain->block + taskId * sizeof(struct task);

		chain->ids[chain->peekCursor] = NOTHING;

		chain->peekCursor++;
		if (chain->peekCursor == chain->numberOfThreads)
			chain->peekCursor = 0;
	}

	pthread_spin_unlock(&chain->lock);

	task->clientIP = clientIP;

	if (task != NULL) {
		task->sockFD = sockFD;
		sem_post(&task->waitForJob);
	}

	return task;
}

void pokeTask(struct task *task)
{
	struct chain *chain = taskChain;

	free(task->clientIP);

	if (task->request == task->response) {
		if (task->request != NULL) {
			pokeBuffer(task->request);
			task->request = NULL;
		}
		task->response = NULL;
	} else {
		if (task->request != NULL) {
			pokeBuffer(task->request);
			task->request = NULL;
		}
		if (task->response != NULL) {
			pokeBuffer(task->response);
			task->response = NULL;
		}
	}

	pthread_spin_lock(&chain->lock);

	chain->ids[chain->pokeCursor] = task->taskId;

	chain->pokeCursor++;
	if (chain->pokeCursor == chain->numberOfThreads)
		chain->pokeCursor = 0;

	pthread_spin_unlock(&chain->lock);
}

struct receiver *peekReceiver(struct task *task)
{
	struct chain *chain = receiverChain;
	struct receiver *receiver;

	pthread_spin_lock(&chain->lock);

	int receiverId = chain->ids[chain->peekCursor];
	if (receiverId == NOTHING) {
		receiver = NULL;
	} else {
		receiver = chain->block + receiverId * sizeof(struct receiver);

		chain->ids[chain->peekCursor] = NOTHING;

		chain->peekCursor++;
		if (chain->peekCursor == chain->numberOfThreads)
			chain->peekCursor = 0;
	}

	pthread_spin_unlock(&chain->lock);

	if (receiver != NULL) {
		receiver->task = task;
		sem_post(&receiver->waitForReadyToGo);
	}

	return receiver;
}

void pokeReceiver(struct receiver *receiver)
{
	struct chain *chain = receiverChain;

#ifdef RESET_SEMAPHORES_IF_NECCESSARY
	int rc = sem_trywait(&receiver->waitForComplete);
	if ((rc == -1) && (errno == EAGAIN)) {
		//fprintf(stderr, "Forgotten receiver semaphore\n");
		sem_post(&receiver->waitForComplete);
	}
#endif

	receiver->task = NULL;

	pthread_spin_lock(&chain->lock);

	chain->ids[chain->pokeCursor] = receiver->receiverId;

	chain->pokeCursor++;
	if (chain->pokeCursor == chain->numberOfThreads)
		chain->pokeCursor = 0;

	pthread_spin_unlock(&chain->lock);
}

struct transmitter *peekTransmitter(struct task *task)
{
	struct chain *chain = transmitterChain;
	struct transmitter *transmitter;

	pthread_spin_lock(&chain->lock);

	int transmitterId = chain->ids[chain->peekCursor];
	if (transmitterId == NOTHING) {
		transmitter = NULL;
	} else {
		transmitter = chain->block + transmitterId * sizeof(struct transmitter);

		chain->ids[chain->peekCursor] = NOTHING;

		chain->peekCursor++;
		if (chain->peekCursor == chain->numberOfThreads)
			chain->peekCursor = 0;
	}

	pthread_spin_unlock(&chain->lock);

	if (transmitter != NULL) {
		transmitter->task = task;
		sem_post(&transmitter->waitForReadyToGo);
	}

	return transmitter;
}

void pokeTransmitter(struct transmitter *transmitter)
{
	struct chain *chain = transmitterChain;

#ifdef RESET_SEMAPHORES_IF_NECCESSARY
	int rc = sem_trywait(&transmitter->waitForComplete);
	if ((rc == -1) && (errno == EAGAIN)) {
		//fprintf(stderr, "Forgotten transmitter semaphore\n");
		sem_post(&transmitter->waitForComplete);
	}
#endif

	transmitter->task = NULL;

	pthread_spin_lock(&chain->lock);

	chain->ids[chain->pokeCursor] = transmitter->transmitterId;

	chain->pokeCursor++;
	if (chain->pokeCursor == chain->numberOfThreads)
		chain->pokeCursor = 0;

	pthread_spin_unlock(&chain->lock);
}

int tasksInUse(void)
{
	pthread_spin_lock(&taskChain->lock);
	int tasksInUse = (taskChain->peekCursor >= taskChain->pokeCursor)
		? taskChain->peekCursor - taskChain->pokeCursor
		: taskChain->numberOfThreads - (taskChain->pokeCursor - taskChain->peekCursor);
	pthread_spin_unlock(&taskChain->lock);
	return tasksInUse;
}

int receiversInUse(void)
{
	pthread_spin_lock(&receiverChain->lock);
	int receiversInUse = (receiverChain->peekCursor >= receiverChain->pokeCursor)
		? receiverChain->peekCursor - receiverChain->pokeCursor
		: receiverChain->numberOfThreads - (receiverChain->pokeCursor - receiverChain->peekCursor);
	pthread_spin_unlock(&receiverChain->lock);
	return receiversInUse;
}

int transmittersInUse(void)
{
	pthread_spin_lock(&transmitterChain->lock);
	int transmittersInUse = (transmitterChain->peekCursor >= transmitterChain->pokeCursor)
		? transmitterChain->peekCursor - transmitterChain->pokeCursor
		: transmitterChain->numberOfThreads - (transmitterChain->pokeCursor - transmitterChain->peekCursor);
	pthread_spin_unlock(&transmitterChain->lock);
	return transmittersInUse;
}
