#ifndef __TASK_KERNEL__
#define __TASK_KERNEL__

int
receiveFixed(
	struct task *task,
	char *buffer,
	ssize_t expectedSize);

int
sendFixed(
	struct task *task,
	char *buffer,
	ssize_t bytesToSend);

int
receivePaquet(struct paquet *paquet, struct buffer *receiveBuffer);

int
sendPaquet(struct paquet *paquet);

#endif
