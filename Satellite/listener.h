#ifndef __LISTENER__
#define __LISTENER__

// Timeout values all multiplied by 1000 to give a better overview
// over millisecond values.
//
#define SLEEP_ON_CANNOT_OPEN_SOCKET             1000  // Milliseconds
#define SLEEP_ON_CANNOT_BIND_SOCKET             1000  // Milliseconds
#define SLEEP_ON_CANNOT_ACCEPT                   500  // Milliseconds
#define SLEEP_ON_SET_SOCKET_OPTIONS              250  // Milliseconds

/**
 * ListenerThread()
 *
 * @arg:
 */
void *
ListenerThread(void *arg);

#endif
