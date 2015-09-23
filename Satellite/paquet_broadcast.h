#ifndef _PAQUET_BROADCAST_
#define _PAQUET_BROADCAST_

#include "paquet.h"

int
paquetBroadcastForOnRadar(struct paquet *paquet);

int
paquetBroadcastForInSight(struct paquet *paquet);

int
paquetBroadcastForOnMap(struct paquet *paquet);

int
paquetDisplacementOnRadar(struct paquet *paquet);

int
paquetDisplacementInSight(struct paquet *paquet);

#endif
