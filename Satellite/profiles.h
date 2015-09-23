#ifndef _PROFILES_
#define _PROFILES_

#include "paquet.h"

int
getProfiles(struct paquet *paquet);

uint64
profileIdByToken(struct dbh *dbh, char *profileToken);

#endif
