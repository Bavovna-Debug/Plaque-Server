#pragma once

#include <c.h>
#include <libpq-fe.h>
#include <pthread.h>

#define BOOLOID                     16
#define BYTEAOID                    17
#define CHAROID                     18
#define INT8OID                     20
#define INT2OID                     21
#define INT4OID                     23
#define TEXTOID                     25
#define XMLOID                      142
#define FLOAT4OID                   700
#define FLOAT8OID                   701
#define INETOID                     869
#define VARCHAROID                  1043
#define UUIDOID                     2950

#define UUIDBinarySize              16

#define UNIQUE_VIOLATION            "23505"
#define CHECK_VIOLATION             "23514"

#define DB_CHAIN_NAME_LENGTH        16

#define DB_MAX_NUMBER_OF_ARGUMENTS  20

struct DB_Chain
{
    char                chainName[DB_CHAIN_NAME_LENGTH];
    pthread_spinlock_t  lock;
    unsigned int        numberOfConnections;
    unsigned int        peekCursor;
    unsigned int        pokeCursor;
    char                conninfo[255];
    void                *block;
    unsigned int        ids[];
};

struct dbh
{
    struct DB_Chain     *chain;
    unsigned int        dbhId;
    PGconn              *conn;
    PGresult            *result;

    struct
    {
        int             numberOfArguments;
        const char      *values  [DB_MAX_NUMBER_OF_ARGUMENTS];
        Oid             types    [DB_MAX_NUMBER_OF_ARGUMENTS];
        int             lengths  [DB_MAX_NUMBER_OF_ARGUMENTS];
        int             formats  [DB_MAX_NUMBER_OF_ARGUMENTS];
    }
    arguments;
};

/**
 * DB_InitChain()
 *
 * @chainName:
 * @numberOfConnections:
 * @conninfo:
 */
struct DB_Chain *
DB_InitChain(
    const char      *chainName,
    unsigned int    numberOfConnections,
    char            *conninfo);

/**
 * DB_ReleaseChain()
 *
 * @chain:
 */
void
DB_ReleaseChain(struct DB_Chain *chain);

/**
 * DB_PeekHandle()
 *
 * @chain:
 */
struct dbh *
DB_PeekHandle(struct DB_Chain *chain);

/**
 * DB_PokeHandle()
 *
 * @dbh:
 */
void
DB_PokeHandle(struct dbh* dbh);

/**
 * DB_ResetHandle()
 *
 * @dbh:
 */
void
DB_ResetHandle(struct dbh* dbh);

/**
 * DB_HasState()
 *
 * @result:
 * @checkState:
 */
inline int
DB_HasState(PGresult *result, const char *checkState);

/**
 * __TuplesOK()
 *
 * @functionName:
 * @dbh:
 * @result:
 */
inline int
__TuplesOK(
    const char  *functionName,
    struct dbh  *dbh,
    PGresult    *result);

/**
 * DB_TuplesOK()
 *
 * @dbh:
 * @result:
 */
#define DB_TuplesOK(dbh, result) \
    __TuplesOK(__FUNCTION__, dbh, result)

/**
 * __CommandOK()
 *
 * @functionName:
 * @dbh:
 * @result:
 */
inline int
__CommandOK(
    const char  *functionName,
    struct dbh  *dbh,
    PGresult    *result);

/**
 * DB_CommandOK()
 *
 * @dbh:
 * @result:
 */
#define DB_CommandOK(dbh, result) \
    __CommandOK(__FUNCTION__, dbh, result)

/**
 * __CorrectNumberOfColumns()
 *
 * @functionName:
 * @result:
 * @expectedNumberOfColumns:
 */
inline int
__CorrectNumberOfColumns(
    const char  *functionName,
    PGresult    *result,
    int         expectedNumberOfColumns);

/**
 * DB_CorrectNumberOfColumns()
 *
 * @result:
 * @expectedNumberOfColumns:
 */
#define DB_CorrectNumberOfColumns(result, expectedNumberOfColumns) \
    __CorrectNumberOfColumns(__FUNCTION__, result, expectedNumberOfColumns)

/**
 * __CorrectNumberOfRows()
 *
 * @functionName:
 * @result:
 * @expectedNumberOfRows:
 */
inline int
__CorrectNumberOfRows(
    const char  *functionName,
    PGresult    *result,
    int         expectedNumberOfRows);

/**
 * DB_CorrectNumberOfRows()
 *
 * @result:
 * @expectedNumberOfRows:
 */
#define DB_CorrectNumberOfRows(result, expectedNumberOfRows) \
    __CorrectNumberOfRows(__FUNCTION__, result, expectedNumberOfRows)

/**
 * __CorrectColumnType()
 *
 * @functionName:
 * @result:
 * @columnNumber:
 * @expectedColumnType:
 */
inline int
__CorrectColumnType(
    const char  *functionName,
    PGresult    *result,
    int         columnNumber,
    Oid         expectedColumnType);

/**
 * DB_CorrectColumnType()
 *
 * @result:
 * @columnNumber:
 * @expectedColumnType:
 */
#define DB_CorrectColumnType(result, columnNumber, expectedColumnType) \
    __CorrectColumnType(__FUNCTION__, result, columnNumber, expectedColumnType)

/**
 * DB_Execute()
 *
 * @dbh:
 * @query:
 */
inline void
DB_Execute(struct dbh *dbh, const char *query);

/**
 * DB_PushArgument()
 *
 * @dbh:
 * @value:
 * @type:
 * @length:
 * @format:
 */
inline void
DB_PushArgument(struct dbh *dbh, char *value, Oid type, int length, int format);

/**
 * DB_PushBIGINT()
 *
 * @dbh:
 * @value:
 */
inline void
DB_PushBIGINT(struct dbh *dbh, uint64 *value);

/**
 * DB_PushINTEGER()
 *
 * @dbh:
 * @value:
 */
inline void
DB_PushINTEGER(struct dbh *dbh, uint32 *value);

/**
 * DB_PushDOUBLE()
 *
 * @dbh:
 * @value:
 */
inline void
DB_PushDOUBLE(struct dbh *dbh, double *value);

/**
 * DB_PushREAL()
 *
 * @dbh:
 * @value:
 */
inline void
DB_PushREAL(struct dbh *dbh, float *value);

/**
 * DB_PushCHAR()
 *
 * @dbh:
 * @value:
 * @length:
 */
inline void
DB_PushCHAR(struct dbh *dbh, char *value, int length);

/**
 * DB_PushVARCHAR()
 *
 * @dbh:
 * @value:
 * @length:
 */
inline void
DB_PushVARCHAR(struct dbh *dbh, char *value, int length);

/**
 * DB_PushBYTEA()
 *
 * @dbh:
 * @value:
 * @length:
 */
inline void
DB_PushBYTEA(struct dbh *dbh, char *value, int length);

/**
 * DB_PushUUID()
 *
 * @dbh:
 * @value:
 */
inline void
DB_PushUUID(struct dbh *dbh, char *value);

/**
 * DB_GetUInt64()
 *
 * @result:
 * @rowNumber:
 * @columnNumber:
 */
inline uint64
DB_GetUInt64(PGresult *result, int rowNumber, int columnNumber);

/**
 * DB_HanldesInUse()
 *
 * @chain:
 */
int
DB_HanldesInUse(struct DB_Chain *chain);

//        PGRES_EMPTY_QUERY = 0,          /* empty query string was executed */
//        PGRES_COMMAND_OK,                       /* a query command that doesn't return
//                                                                 * anything was executed properly by the
//                                                                 * backend */
//        PGRES_TUPLES_OK,                        /* a query command that returns tuples was
//                                                                 * executed properly by the backend, PGresult
//                                                                 * contains the result tuples */
//        PGRES_COPY_OUT,                         /* Copy Out data transfer in progress */
//        PGRES_COPY_IN,                          /* Copy In data transfer in progress */
//        PGRES_BAD_RESPONSE,                     /* an unexpected response was recv'd from the
//                                                                 * backend */
//        PGRES_NONFATAL_ERROR,           /* notice or warning message */
//        PGRES_FATAL_ERROR,                      /* query failed */
//        PGRES_COPY_BOTH,                        /* Copy In/Out data transfer in progress */
//        PGRES_SINGLE_TUPLE
