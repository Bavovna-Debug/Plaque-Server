#ifndef _SESSION_
#define _SESSION_

#include <c.h>

#include "db.h"
#include "tasks.h"

uint64
getSessionForDevice(
    struct task *task,
    struct dbh *dbh,
    uint64 deviceId,
    uint64 *sessionId,
    char *knownSessionToken,
    char *givenSessionToken);

int
setAllSessionsOffline(struct desk *desk);

int
setSessionOnline(struct task *task);

int
setSessionOffline(struct task *task);

int
getSessionNextOnRadarRevision(
    struct task *task,
    struct dbh *dbh,
    uint32 *nextOnRadarRevision);

int
getSessionNextInSightRevision(
    struct task *task,
    struct dbh *dbh,
    uint32 *nextInSightRevision);

#endif
