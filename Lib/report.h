#ifndef __REPORT__
#define __REPORT__

#ifdef SYSLOG
#include <syslog.h>
#else
#ifdef PGBGW
#include <postgres.h>
#else
#include <stdio.h>
#include <time.h>
#endif
#endif

// ReportPanic:
// A panic condition. Application breaks the execution immediately.
//
// ReportSoftAlert:
// A software condition that should be corrected immediately, such as a corrupted data.
//
// ReportHardAlert:
// A hardware condition that should be corrected immediately, such as device errors.
//
// ReportError:
// Error message. Some system component has broken its usual execution but the system goes on.
//
// ReportWarning:
// Warning message. System continues its normal execution.
//
// ReportInfo:
// Informational message.
//
// ReportDebug:
// Debug message that may be reported if the code is compiled with debug option.

#ifdef SYSLOG

#define ReportPanic(...) \
    syslog(LOG_EMERG, __VA_ARGS__)

#define ReportSoftAlert(...) \
    syslog(LOG_ALERT, __VA_ARGS__)

#define ReportHardAlert(...) \
    syslog(LOG_CRIT, __VA_ARGS__)

#define ReportError(...) \
    syslog(LOG_ERR, __VA_ARGS__)

#define ReportWarning(...) \
    syslog(LOG_WARNING, __VA_ARGS__)

#define ReportInfo(...) \
    syslog(LOG_INFO, __VA_ARGS__)

#define ReportDebug(...) \
    syslog(LOG_DEBUG, __VA_ARGS__)

#else
#ifdef PGBGW

#define ReportPanic(...) \
    elog(LOG, __VA_ARGS__)

#define ReportSoftAlert(...) \
    elog(LOG, __VA_ARGS__)

#define ReportHardAlert(...) \
    elog(LOG, __VA_ARGS__)

#define ReportError(...) \
    elog(LOG, __VA_ARGS__)

#define ReportWarning(...) \
    elog(LOG, __VA_ARGS__)

#define ReportInfo(...) \
    elog(LOG, __VA_ARGS__)

#define ReportDebug(...) \
    elog(LOG, __VA_ARGS__)

#else

#define ReportPanic(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [PANIC] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define ReportSoftAlert(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [ALERT] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define ReportHardAlert(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [ALERT] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define ReportError(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [ERROR] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define ReportWarning(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [INFO] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define ReportInfo(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [INFO] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#define ReportDebug(...) \
    do { \
        struct timespec time; \
        clock_gettime(CLOCK_REALTIME, &time); \
        fprintf(stdout, "%lu.%09lu [DEBUG] ", time.tv_sec, time.tv_nsec); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    } while (0)

#endif
#endif

#endif
