#ifndef __REPORT__
#define __REPORT__

#ifdef PGBGW
#include <postgres.h>
#else
#include <stdio.h>
#include <time.h>
#endif

// reportPanic:
// A panic condition. Application breaks the execution immediately.
//
// reportSoftAlert:
// A software condition that should be corrected immediately, such as a corrupted data.
//
// reportHardAlert:
// A hardware condition that should be corrected immediately, such as device errors.
//
// reportError:
// Error message. Some system component has broken its usual execution but the system goes on.
//
// reportWarning:
// Warning message. System continues its normal execution.
//
// reportInfo:
// Informational message.
//
// reportDebug:
// Debug message that may be reported if the code is compiled with debug option.

#ifdef PGBGW

#define reportPanic(...) \
    elog(LOG, __VA_ARGS__)

#define reportSoftAlert(...) \
    elog(LOG, __VA_ARGS__)

#define reportHardAlert(...) \
    elog(LOG, __VA_ARGS__)

#define reportError(...) \
    elog(LOG, __VA_ARGS__)

#define reportWarning(...) \
    elog(LOG, __VA_ARGS__)

#define reportInfo(...) \
    elog(LOG, __VA_ARGS__)

#define reportDebug(...) \
    elog(LOG, __VA_ARGS__)

#else

#define reportPanic(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [PANIC] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define reportSoftAlert(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [ALERT] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define reportHardAlert(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [ALERT] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define reportError(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [ERROR] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define reportWarning(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [INFO] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define reportInfo(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [INFO] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define reportDebug(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [DEBUG] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#endif

#endif
