#ifndef __DESK__
#define __DESK__

#include "broadcaster_api.h"
#include "mmps.h"
#include "db.h"
#include "tasks.h"

#define undef

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

#define BUFFER_DIALOGUE_PAQUET      0xDD000000
#define BUFFER_DIALOGUE_FIRST       0xDD000001
#define BUFFER_DIALOGUE_FOLLOWING   0xDD000002
#define BUFFER_XMIT                 0xEE000000
#define BUFFER_TASK                 0x22000000
#define BUFFER_BROADCAST            0xBB000000
#define BUFFER_PROFILES             0xFF000000
#define BUFFER_PLAQUES              0xAA000000

typedef struct desk {
    struct {
        struct MMPS_Pool    *task;
        struct MMPS_Pool    *paquet;
        struct MMPS_Pool    *dynamic;
    } pools;

    struct {
        struct dbChain      *guardian;
        struct dbChain      *auth;
        struct dbChain      *plaque;
    } dbh;

    struct {
        void                **list;
    } tasks;

    struct {
        uint16_t            portNumber;
        struct session      session;
    } broadcaster;

    struct {
        uint16_t            portNumber;
        int                 listenSockFD;
    } listener;
} desk_t;

#endif
