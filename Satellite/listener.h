#ifndef _LISTENER_
#define _LISTENER_

#define SLEEP_ON_CANNOT_OPEN_SOCKET                1 * 1000 * 1000  // Microseconds
#define SLEEP_ON_CANNOT_BIND_SOCKET                1 * 1000 * 1000  // Microseconds
#define SLEEP_ON_CANNOT_ACCEPT                          500         // Microseconds

void *
listenerThread(void *arg);

#endif
