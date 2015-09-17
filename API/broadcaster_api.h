#ifndef _BROADCASTER_API_
#define _BROADCASTER_API_

#include <c.h>

#define BROADCASTER_PORT_NUMBER             20000

typedef struct session {
    uint64              receiptId;
    uint64              sessionId;
    uint32              inCacheRevision;
    uint32              onRadarRevision;
    uint32              inSightRevision;
} session_t;

#endif
