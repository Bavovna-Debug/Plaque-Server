#ifndef __DBH__
#define __DBH__

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

#define UUIDBinarySize			16

#define UNIQUE_VIOLATION		"23505"
#define CHECK_VIOLATION			"23514"

#define DB_CHAIN_NAME_LENGTH	16

typedef struct dbChain {
	char				chainName[DB_CHAIN_NAME_LENGTH];
	pthread_spinlock_t	lock;
	unsigned int		numberOfConnections;
	unsigned int		peekCursor;
	unsigned int		pokeCursor;
	char				conninfo[255];
	void				*block;
	unsigned int		ids[];
} dbChain_t;

typedef struct dbh {
	struct dbChain		*chain;
	unsigned int		dbhId;
	PGconn				*conn;
	PGresult			*result;
	struct arguments {
		int				numberOfArguments;
		const char		*values  [8];
    	Oid				types    [8];
		int				lengths  [8];
		int				formats  [8];
	} arguments;
} dbh_t;


struct dbChain *
initDBChain(const char *chainName, unsigned int numberOfConnections, char *conninfo);

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
__dbhTuplesOK(
	const char	*functionName,
	struct dbh	*dbh,
	PGresult	*result);

#define dbhTuplesOK(dbh, result) \
	__dbhTuplesOK(__FUNCTION__, dbh, result)

inline int
__dbhCommandOK(
	const char	*functionName,
	struct dbh	*dbh,
	PGresult	*result);

#define dbhCommandOK(dbh, result) \
	__dbhCommandOK(__FUNCTION__, dbh, result)

inline int
__dbhCorrectNumberOfColumns(
	const char	*functionName,
	PGresult	*result,
	int			expectedNumberOfColumns);

#define dbhCorrectNumberOfColumns(result, expectedNumberOfColumns) \
	__dbhCorrectNumberOfColumns(__FUNCTION__, result, expectedNumberOfColumns)

inline int
__dbhCorrectNumberOfRows(
	const char	*functionName,
	PGresult	*result,
	int			expectedNumberOfRows);

#define dbhCorrectNumberOfRows(result, expectedNumberOfRows) \
	__dbhCorrectNumberOfRows(__FUNCTION__, result, expectedNumberOfRows)

inline int
__dbhCorrectColumnType(
	const char	*functionName,
	PGresult	*result,
	int 		columnNumber,
	Oid			expectedColumnType);

#define dbhCorrectColumnType(result, columnNumber, expectedColumnType) \
	__dbhCorrectColumnType(__FUNCTION__, result, columnNumber, expectedColumnType)

inline void
dbhExecute(struct dbh *dbh, const char *query);

inline void
dbhPushArgument(struct dbh *dbh, char *value, Oid type, int length, int format);

inline void
dbhPushBIGINT(struct dbh *dbh, uint64 *value);

inline void
dbhPushINTEGER(struct dbh *dbh, uint32 *value);

inline void
dbhPushDOUBLE(struct dbh *dbh, double *value);

inline void
dbhPushREAL(struct dbh *dbh, float *value);

inline void
dbhPushCHAR(struct dbh *dbh, char *value, int length);

inline void
dbhPushVARCHAR(struct dbh *dbh, char *value, int length);

inline void
dbhPushBYTEA(struct dbh *dbh, char *value, int length);

inline void
dbhPushUUID(struct dbh *dbh, char *value);

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
