#ifndef _SESSION_
#define _SESSION_

#include "db.h"
#include "tasks.h"

uint64_t getSessionForDevice(struct task *task, struct dbh *dbh,
    uint64_t deviceId, uint64_t *sessionId,
    char *knownSessionToken, char *givenSessionToken);

int getSessionNextInSightRevision(struct task *task, struct dbh *dbh, uint32_t *nextInSightRevision);

#endif
