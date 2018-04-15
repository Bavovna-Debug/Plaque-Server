#pragma once

#include "paquet.h"

int
HandlePostNewPlaque(struct Paquet *paquet);

int
HandleChangePlaqueLocation(struct Paquet *paquet);

int
HandleChangePlaqueOrientation(struct Paquet *paquet);

int
HandleChangePlaqueSize(struct Paquet *paquet);

int
HandleChangePlaqueColors(struct Paquet *paquet);

int
HandleChangePlaqueFont(struct Paquet *paquet);

int
HandleChangePlaqueInscription(struct Paquet *paquet);
