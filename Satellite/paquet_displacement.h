#ifndef __PAQUET_DISPLACEMENT__
#define __PAQUET_DISPLACEMENT__

#include "paquet.h"

int
HandleDisplacementOnRadar(struct paquet *paquet);

int
HandleDisplacementInSight(struct paquet *paquet);

int
HandleDisplacementOnMap(struct paquet *paquet);

#endif
