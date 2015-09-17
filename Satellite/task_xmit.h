#ifndef _TASK_KERNEL_
#define _TASK_KERNEL_

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
