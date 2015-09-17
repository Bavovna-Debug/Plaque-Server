#ifndef _DBH_
#define _DBH_

#include <c.h>
#include <libpq-fe.h>
#include <pthread.h>

#define BOOLOID					16
#define BYTEAOID				17
#define CHAROID					18
#define INT8OID					20
#define INT2OID					21
#define INT4OID					23
#define TEXTOID					25
#define XMLOID					142
#define FLOAT4OID				700
#define FLOAT8OID				701
#define INETOID					869
#define VARCHAROID				1043
#define UUIDOID					2950

#define UNIQUE_VIOLATION		"23505"
#define CHECK_VIOLATION			"23514"

#define DB_CHAIN_NAME_LENGTH	16

typedef struct dbChain {
	char				chainName[DB_CHAIN_NAME_LENGTH];
	pthread_spinlock_t	lock;
	int					numberOfConnections;
	int					peekCursor;
	int					pokeCursor;
	char				conninfo[255];
	void				*block;
	int					ids[];
} dbChain_t;

typedef struct dbh {
	struct dbChain		*chain;
	int					dbhId;
	pthread_spinlock_t	lock;
	PGconn				*conn;
	PGresult			*result;
} dbh_t;

struct dbChain *
initDBChain(const char *chainName, int numberOfConnections, char *conninfo);

void
releaseDBChain(struct dbChain *chain);

struct dbh *
peekDB(struct dbChain *chain);

void
pokeDB(struct dbh* dbh);

void
resetDB(struct dbh* dbh);

inline int
sqlState(PGresult *result, const char *checkState);

inline int
dbhTuplesOK(struct dbh *dbh, PGresult *result);

inline int
dbhCommandOK(struct dbh *dbh, PGresult *result);

inline int
dbhCorrectNumberOfColumns(PGresult *result, int expectedNumberOfColumn);

inline int
dbhCorrectNumberOfRows(PGresult *result, int expectedNumberOfRows);

inline int
dbhCorrectColumnType(PGresult *result, int columnNumber, Oid expectedColumnType);

inline uint64
dbhGetUInt64(PGresult *result, int rowNumber, int columnNumber);

int
dbhInUse(struct dbChain *chain);

#endif

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
