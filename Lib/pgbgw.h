#include <pgstat.h>

#define PGBGW_BEGIN \
    do { \
        StartTransactionCommand(); \
        SPI_connect(); \
        PushActiveSnapshot(GetTransactionSnapshot()); \
        pgstat_report_activity(STATE_IDLE, __FUNCTION__); \
    } while (0);

#define PGBGW_COMMIT \
    do { \
        SPI_finish(); \
        PopActiveSnapshot(); \
        CommitTransactionCommand(); \
        pgstat_report_activity(STATE_IDLE, NULL); \
    } while (0);

#define PGBGW_ROLLBACK \
    do { \
        SPI_finish(); \
        PopActiveSnapshot(); \
        AbortCurrentTransaction(); \
        pgstat_report_activity(STATE_IDLE, NULL); \
    } while (0);
