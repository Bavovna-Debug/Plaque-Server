#pragma once

#include "mmps.h"

#define XMIT_MAX_NUMBER_OF_VECTORS 1000

/**
 * FillPaquetWithPilotData()
 *
 * @paquet:
 * @pilot:
 */
inline void
FillPaquetWithPilotData(struct Paquet *paquet);

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
