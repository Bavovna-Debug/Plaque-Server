#ifndef _QUEUE_
#define _QUEUE_

#include "tasks.h"

void constructQueue(void);

void destructQueue(void);

struct task *peekTask(int sockFD, char *clientIP);

void pokeTask(struct task *task);

struct receiver *peekReceiver(struct task *task);

void pokeReceiver(struct receiver *receiver);

struct transmitter *peekTransmitter(struct task *task);

void pokeTransmitter(struct transmitter *transmitter);

int tasksInUse(void);

int receiversInUse(void);

int transmittersInUse(void);

#endif
