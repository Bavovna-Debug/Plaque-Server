#ifndef _PLAQUES_
#define _PLAQUES_

#include "paquet.h"

int paquetPostNewPlaque(struct paquet *paquet);

int paquetChangePlaqueLocation(struct paquet *paquet);

int paquetChangePlaqueOrientation(struct paquet *paquet);

int paquetChangePlaqueSize(struct paquet *paquet);

int paquetChangePlaqueColors(struct paquet *paquet);

int paquetChangePlaqueFont(struct paquet *paquet);

int paquetChangePlaqueInscription(struct paquet *paquet);

#endif
