#ifndef _DESK_
#define _DESK_

#include "broadcaster_api.h"
#include "buffers.h"
#include "db.h"
#include "tasks.h"

#define SANDBOX

#ifdef SANDBOX

#define NUMBER_OF_BUFFERS_TASK		    200L
#define NUMBER_OF_BUFFERS_PAQUET	    200L
#define NUMBER_OF_BUFFERS_256		    200L
#define NUMBER_OF_BUFFERS_512		    200L
#define NUMBER_OF_BUFFERS_1K		    200L
#define NUMBER_OF_BUFFERS_4K		    200L
#define NUMBER_OF_BUFFERS_1M		    200L

#define NUMBER_OF_DBH_GUARDIANS			10
#define NUMBER_OF_DBH_AUTHENTICATION	10
#define NUMBER_OF_DBH_PLAQUES_SESSION	40

#else

#define NUMBER_OF_BUFFERS_TASK		     2000000L
#define NUMBER_OF_BUFFERS_PAQUET	     8000000L
#define NUMBER_OF_BUFFERS_256		     2000000L
#define NUMBER_OF_BUFFERS_512		     2000000L
#define NUMBER_OF_BUFFERS_1K		     6000000L
#define NUMBER_OF_BUFFERS_4K		      200000L
#define NUMBER_OF_BUFFERS_1M	    	     200L

#define NUMBER_OF_DBH_GUARDIANS			      50
#define NUMBER_OF_DBH_AUTHENTICATION	     200
#define NUMBER_OF_DBH_PLAQUES_SESSION	     600

#endif

typedef struct desk {
    struct {
        struct pool     *task;
        struct pool     *paquet;
        struct pool     *dynamic;
    } pools;

    struct {
        struct dbChain  *guardian;
        struct dbChain  *auth;
        struct dbChain  *plaque;
    } dbh;

    struct {
        void            **list;
    } tasks;

    struct {
        uint16_t        portNumber;
        struct session  session;
    } broadcaster;

    struct {
        uint16_t        portNumber;
        int             listenSockFD;
    } listener;
} desk_t;

#endif
