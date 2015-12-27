#ifndef _DESK_
#define _DESK_

#include <pthread.h>

#include "buffers.h"

#define POOL_NOTIFICATIONS_NUMBER_OF_BUFFERS    1000
#define POOL_APNS_NUMBER_OF_BUFFERS             10
#define POOL_APNS_SIZE_OF_BUFFER                32 * KB

#define BUFFER_XMIT         0x00000001
#define BUFFER_NOTIFICATION 0x00000002

typedef struct desk {
    struct {
	    pthread_t           thread;
        pthread_attr_t      attributes;
        pthread_mutex_t     readyToGoMutex;
        pthread_cond_t      readyToGoCond;
    } apns;

    struct {
        struct pool         *notifications;
        struct pool         *apns;
    } pools;

    struct {
        pthread_mutex_t     mutex;
        struct buffer       *buffers;
    } outstandingNotifications;

    struct {
        pthread_mutex_t     mutex;
        struct buffer       *buffers;
    } inTheAirNotifications;

    struct {
        pthread_mutex_t     mutex;
        struct buffer       *buffers;
    } sentNotifications;

    struct {
        pthread_mutex_t     mutex;
        struct buffer       *buffers;
    } processedNotifications;
} desk_t;

#endif
