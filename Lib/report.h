#ifndef _REPORT_
#define _REPORT_

#ifdef PGBGW
#include <postgres.h>
#else
#include <stdio.h>
#endif

#ifdef PGBGW

#define reportLog(...) \
    elog(LOG, __VA_ARGS__)

#define reportError(...) \
    elog(LOG, __VA_ARGS__)

#else

#define reportLog(...) \
    do { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while (0)

#define reportError(...) \
    do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)

#endif

#endif
