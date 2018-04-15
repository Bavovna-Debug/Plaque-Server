#pragma once

#include "paquet.h"

int
GetProfiles(struct Paquet *paquet);

uint64
ProfileIdByToken(struct dbh *dbh, char *profileToken);
