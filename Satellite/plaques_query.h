#ifndef _PLAQUES_QUERY_
#define _PLAQUES_QUERY_

#include "paquet.h"

int
paquetListOfPlaquesForBroadcast(struct paquet *paquet);

int
paquetListOfPlaquesOnRadar(struct paquet *paquet);

int
paquetListOfPlaquesInSight(struct paquet *paquet);

int
paquetDownloadPlaques(struct paquet *paquet);

#endif
