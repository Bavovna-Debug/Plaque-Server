#pragma once

#include <c.h>

#include "db.h"
#include "tasks.h"

int
GetSessionForDevice(
    struct Task *task,
    struct dbh *dbh,
    uint64 deviceId,
    uint64 *sessionId,
    char *knownSessionToken,
    char *givenSessionToken);

int
SetAllSessionsOffline(void);

int
SetSessionOnline(struct Task *task);

int
SetSessionOffline(struct Task *task);

int
GetSessionRevisions(
    struct Task         *task,
    struct Revisions    *revisions);

int
GetSessionOnRadarRevision(
    struct Task *task,
    struct dbh  *dbh,
    uint32      *revision);

int
GetSessionInSightRevision(
    struct Task *task,
    struct dbh  *dbh,
    uint32      *revision);

int
GetSessionOnMapRevision(
    struct Task *task,
    struct dbh  *dbh,
    uint32      *revision);
