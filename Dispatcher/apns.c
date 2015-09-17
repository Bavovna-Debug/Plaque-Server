#include <postgres.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/lwlock.h>
#include <storage/proc.h>
#include <storage/shmem.h>
#include <access/xact.h>
#include <executor/spi.h>
#include <fmgr.h>
#include <lib/stringinfo.h>
#include <pgstat.h>
#include <utils/builtins.h>
#include <utils/snapmgr.h>
#include <tcop/utility.h>

#include "apns.h"

int
delivery(void)
{
    elog(LOG, "Dispatcher idle");

    return 0;
}
