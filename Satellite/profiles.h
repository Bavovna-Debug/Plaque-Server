#ifndef __PROFILES__
#define __PROFILES__

#include "paquet.h"

int
getProfiles(struct paquet *paquet);

uint64
profileIdByToken(struct dbh *dbh, char *profileToken);

#endif
