#ifndef __TASK_XMIT__
#define __TASK_XMIT__

#include "mmps.h"

/**
 * FillPaquetWithPilotData()
 *
 * @paquet:
 * @pilot:
 */
inline void
FillPaquetWithPilotData(struct paquet *paquet, struct PaquetPilot *pilot);

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
receivePaquet(struct paquet *paquet, struct MMPS_Buffer *receiveBuffer);

int
sendPaquet(struct paquet *paquet);

#endif
