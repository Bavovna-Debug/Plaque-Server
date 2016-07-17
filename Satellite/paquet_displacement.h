#ifndef __PAQUET_DISPLACEMENT__
#define __PAQUET_DISPLACEMENT__

#include "paquet.h"

int
HandleDisplacementOnRadar(struct Paquet *paquet);

int
HandleDisplacementInSight(struct Paquet *paquet);

int
HandleDisplacementOnMap(struct Paquet *paquet);

#endif
