#ifndef _DESK_
#define _DESK_

#include <pthread.h>
#include <semaphore.h>

#include "buffers.h"

#define POOL_NOTIFICATIONS_NUMBER_OF_BUFFERS    1000
#define POOL_APNS_NUMBER_OF_BUFFERS             10
#define POOL_APNS_SIZE_OF_BUFFER                MB

typedef struct desk {
    struct {
	    pthread_t           thread;
        sem_t               *readyToGo;
    } apns;

    struct {
        struct pool         *notifications;
        struct pool         *apns;
    } pools;

    struct {
    	pthread_spinlock_t  lock;
        struct buffer       *buffers;
    } outstandingNotifications;

    struct {
    	pthread_spinlock_t  lock;
        struct buffer       *buffers;
    } inTheAirNotifications;

    struct {
    	pthread_spinlock_t  lock;
        struct buffer       *buffers;
    } sentNotifications;

    struct {
    	pthread_spinlock_t  lock;
        struct buffer       *buffers;
    } processedNotifications;
} desk_t;

#endif
