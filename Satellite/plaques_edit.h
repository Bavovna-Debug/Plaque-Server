#ifndef __PLAQUES__
#define __PLAQUES__

#include "paquet.h"

int
HandlePostNewPlaque(struct paquet *paquet);

int
HandleChangePlaqueLocation(struct paquet *paquet);

int
HandleChangePlaqueOrientation(struct paquet *paquet);

int
HandleChangePlaqueSize(struct paquet *paquet);

int
HandleChangePlaqueColors(struct paquet *paquet);

int
HandleChangePlaqueFont(struct paquet *paquet);

int
HandleChangePlaqueInscription(struct paquet *paquet);

#endif
