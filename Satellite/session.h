#ifndef __SESSION__
#define __SESSION__

#include <c.h>

#include "db.h"
#include "tasks.h"

int
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
getSessionRevisions(
    struct task         *task,
    struct revisions    *revisions);

int
getSessionOnRadarRevision(
    struct task *task,
    struct dbh  *dbh,
    uint32      *revision);

int
getSessionInSightRevision(
    struct task *task,
    struct dbh  *dbh,
    uint32      *revision);

int
getSessionOnMapRevision(
    struct task *task,
    struct dbh  *dbh,
    uint32      *revision);

#endif
