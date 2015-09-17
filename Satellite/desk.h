#ifndef _DESK_
#define _DESK_

#include "broadcaster_api.h"
#include "buffers.h"
#include "db.h"
#include "tasks.h"

#define TEST

#ifdef TEST

#define NUMBER_OF_BUFFERS_TASK		    20L
#define NUMBER_OF_BUFFERS_PAQUET	    20L
#define NUMBER_OF_BUFFERS_1K		    20L
#define NUMBER_OF_BUFFERS_4K		    10L
#define NUMBER_OF_BUFFERS_1M		    10L

#define NUMBER_OF_DBH_GUARDIANS			10//2000
#define NUMBER_OF_DBH_AUTHENTICATION	10//2000
#define NUMBER_OF_DBH_PLAQUES_SESSION	10//4000

#else

#define NUMBER_OF_BUFFERS_TASK		    20000000L
#define NUMBER_OF_BUFFERS_PAQUET	    30000000L
#define NUMBER_OF_BUFFERS_1K		    10000000L
#define NUMBER_OF_BUFFERS_4K		      200000L
#define NUMBER_OF_BUFFERS_1M	    	    2000L

#define NUMBER_OF_DBH_GUARDIANS			2000
#define NUMBER_OF_DBH_AUTHENTICATION	2000
#define NUMBER_OF_DBH_PLAQUES_SESSION	4000

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
    } listener;
} desk_t;

#endif
