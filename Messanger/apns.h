#ifndef _APNS_
#define _APNS_

#include "desk.h"

#define APNS_DISCONNECT_IF_IDLE         60
#define SLEEP_ON_CONNECT_ERROR          5
#define SLEEP_ON_XMIT_ERROR             10
#define SLEEP_ON_BUSY_RESOURCES         2
#define SLEEP_ON_OTHER_ERROR            10
#define APNS_MAX_PAYLOAD_LENGTH         2048

void *
apnsThread(void *arg);

void
apnsKnockKnock(struct desk *desk);

#endif
