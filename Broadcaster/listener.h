#ifndef _LISTENER_
#define _LISTENER_

#include "desk.h"

void *
listenerThread(void *arg);

void
listenerKnockKnock(struct desk *desk);

#endif
