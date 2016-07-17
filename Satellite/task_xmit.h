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
FillPaquetWithPilotData(struct Paquet *paquet, struct PaquetPilot *pilot);

int
ReceiveFixed(
	struct Task *task,
	char *buffer,
	ssize_t expectedSize);

int
SendFixed(
	struct Task *task,
	char *buffer,
	ssize_t bytesToSend);

int
ReceivePaquet(struct Paquet *paquet, struct MMPS_Buffer *receiveBuffer);

int
SendPaquet(struct Paquet *paquet);

#endif
