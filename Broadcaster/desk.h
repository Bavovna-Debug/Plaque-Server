#ifndef _DESK_
#define _DESK_

#include <pthread.h>
#include <semaphore.h>

#include "broadcaster_api.h"

#define MAX_REVISED_SESSIONS_PER_STEP       100

#define SLEEP_WHEN_LISTENER_IS_BUSY                2                // Seconds
#define SLEEP_ON_CANNOT_OPEN_SOCKET                1 * 1000 * 1000  // Microseconds
#define SLEEP_ON_CANNOT_BIND_SOCKET                1 * 1000 * 1000  // Microseconds
#define SLEEP_ON_CANNOT_ACCEPT                          500 * 1000  // Microseconds
#define TIMEOUT_DISCONNECT_IF_IDLE               300                // Seconds
#define TIMEOUT_ON_WAIT_FOR_BEGIN_TO_TRANSMIT     10 * 1000 * 1000	// Milliseconds
#define TIMEOUT_ON_POLL_FOR_RECEIPT                5 * 1000 * 1000	// Milliseconds

typedef struct desk {
    struct {
        uint16_t            portNumber;
	    pthread_t           thread;
        sem_t               *readyToGo;
    } listener;

    struct {
	    pthread_spinlock_t	lock;
        uint32              numberOfSessions;
        struct session      sessions[MAX_REVISED_SESSIONS_PER_STEP];
        uint64              lastReceiptId;
    } watchdog;
} desk_t;

#endif
