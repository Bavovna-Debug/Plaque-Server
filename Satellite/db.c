#include <c.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "report.h"

#define PEEK_DBH_RETRIES				5
#define PEEK_DBH_RETRY_SLEEP			1500000

#define NOTHING							-1

void
connectToPostgres(struct dbh *dbh)
{
	// Connect to database.
	//
	//dbh->conn = PQsetdbLogin(PLAQUE_DB_HOST, PLAQUE_DB_PORT, NULL, NULL, PLAQUE_DB_DATABASE, PLAQUE_DB_USERNAME, PLAQUE_DB_PASSWORT);
	dbh->conn = PQconnectdb(dbh->chain->conninfo);

	// Check to see that the backend connection was successfully made.
	//
	if (PQstatus(dbh->conn) != CONNECTION_OK) {
		reportError("Connection to database failed: %s",
				PQerrorMessage(dbh->conn));

		dbh->conn = NULL;
	}
}

void
disconnectFromPostgres(struct dbh *dbh)
{
	// Disconnect from database and cleanup.
	//
	PQfinish(dbh->conn);

	dbh->conn = NULL;
}

struct dbChain *
initDBChain(const char *chainName, int numberOfConnections, char *conninfo)
{
	struct dbChain *chain = malloc(sizeof(struct dbChain) + numberOfConnections * sizeof(int));
	if (chain == NULL) {
        reportError("Out of memory");
        return NULL;
    }

	chain->block = malloc(numberOfConnections * sizeof(struct dbh));
	if (chain->block == NULL) {
		reportError("Out of memory");
		return NULL;
	}

	strncpy(chain->chainName, chainName, DB_CHAIN_NAME_LENGTH);

	pthread_spin_init(&chain->lock, PTHREAD_PROCESS_PRIVATE);

	chain->numberOfConnections = numberOfConnections;
	chain->peekCursor = 0;
	chain->pokeCursor = 0;
	strncpy(chain->conninfo, conninfo, sizeof(chain->conninfo));

	int dbhId;
	for (dbhId = 0; dbhId < chain->numberOfConnections; dbhId++)
	{
		struct dbh *dbh = chain->block + dbhId * sizeof(struct dbh);

		dbh->chain = chain;

		dbh->dbhId = dbhId;

		dbh->result = NULL;
	}

	for (dbhId = 0; dbhId < chain->numberOfConnections; dbhId++)
		chain->ids[dbhId] = dbhId;

	for (dbhId = 0; dbhId < chain->numberOfConnections; dbhId++)
	{
		struct dbh *dbh = chain->block + dbhId * sizeof(struct dbh);
		connectToPostgres(dbh);
	}

	return chain;
}

void
releaseDBChain(struct dbChain *chain)
{
	int dbhId;

	for (dbhId = 0; dbhId < chain->numberOfConnections; dbhId++)
	{
		struct dbh *dbh = chain->block + dbhId * sizeof(struct dbh);

		disconnectFromPostgres(dbh);
	}

	pthread_spin_destroy(&chain->lock);

	free(chain->block);

	free(chain);
}

struct dbh *
peekDB(struct dbChain *chain)
{
#ifdef DB_PEEK_POKE
	reportLog("Peek DB from chain %s", chain->chainName);
#endif

	struct dbh* dbh;

	int try;
	for (try = 0; try < PEEK_DBH_RETRIES; try++)
	{
		pthread_spin_lock(&chain->lock);

		int dbhId = chain->ids[chain->peekCursor];
		if (dbhId == NOTHING) {
			dbh = NULL;
		} else {
			dbh = chain->block + dbhId * sizeof(struct dbh);

			chain->ids[chain->peekCursor] = NOTHING;

			chain->peekCursor++;
			if (chain->peekCursor == chain->numberOfConnections)
				chain->peekCursor = 0;
		}

		pthread_spin_unlock(&chain->lock);

		if (dbh == NULL) {
			usleep(PEEK_DBH_RETRY_SLEEP);
		} else {
			break;
		}
	}

	if (dbh != NULL) {
		//
		// Connect or disconnect if needed.
		//
		if (dbh->conn == NULL) {
			connectToPostgres(dbh);
		} else {
			if (PQstatus(dbh->conn) != CONNECTION_OK) {
				disconnectFromPostgres(dbh);
				connectToPostgres(dbh);
			}
		}

		// Could not connect to DB? Then quit.
		//
		if (dbh->conn == NULL) {
			pokeDB(dbh);
			return NULL;
		}

		PGresult *result;

		// Start the transaction block.
		//
		result = PQexec(dbh->conn, "BEGIN");
		if (PQresultStatus(result) != PGRES_COMMAND_OK) {
			//
			// Perhaps connection is being lost since this DB handler was last in use.
			// Try to reconnect now.
			//
			disconnectFromPostgres(dbh);
			connectToPostgres(dbh);

			if ((dbh->conn == NULL) || (PQstatus(dbh->conn) != CONNECTION_OK)) {
				pokeDB(dbh);
				dbh = NULL;
			} else {
				PQclear(result);
				result = PQexec(dbh->conn, "BEGIN");
				if (PQresultStatus(result) != PGRES_COMMAND_OK) {
					//
					// If still cannot connect to Db, then fail.
					//
					pokeDB(dbh);
					reportError("Start transaction failed: %s",
						PQerrorMessage(dbh->conn));
					dbh = NULL;
				}
			}
		}

		PQclear(result);

		dbh->arguments.numberOfArguments = 0;
	}

	if (dbh == NULL)
		reportLog("No database handler available");

	return dbh;
}

void
pokeDB(struct dbh *dbh)
{
#ifdef DB_PEEK_POKE
	reportLog("Poke DB %d to chain %s (last status %d)",
		dbh->dbhId,
		dbh->chain->chainName,
		(dbh->result == NULL) ? -1 : PQresultStatus(dbh->result));
#endif

	struct dbChain *chain = dbh->chain;

	if (dbh->result != NULL) {
		//
		// Last command was executed successfully.
		// Therefore assume that connection is still in a good state.
		//
		PQclear(dbh->result);
		dbh->result = NULL;

		if (PQstatus(dbh->conn) == CONNECTION_OK) {
			//
			// Commit transaction.
			//
			PGresult *result = PQexec(dbh->conn, "COMMIT");
			PQclear(result);
		}
	} else {
		//
		// Otherwise disconnect this DB handler from DB so that it will be connected on next use.
		//
//		disconnectFromPostgres(dbh);
	}

	pthread_spin_lock(&chain->lock);

	chain->ids[chain->pokeCursor] = dbh->dbhId;

	chain->pokeCursor++;
	if (chain->pokeCursor == chain->numberOfConnections)
		chain->pokeCursor = 0;

	pthread_spin_unlock(&chain->lock);
}

void
resetDB(struct dbh *dbh)
{
	// Rollback all previous transactions.
	//
	PGresult *result = PQexec(dbh->conn, "ROLLBACK");
	PQclear(result);

	PQreset(dbh->conn);
}

inline int
sqlState(PGresult *result, const char *checkState)
{
	char *sqlStateString = PQresultErrorField(result, PG_DIAG_SQLSTATE);
	if (sqlStateString == NULL) {
		return 0;
	} else {
		if (strncmp(sqlStateString, checkState, 5) == 0) {
			return 1;
		} else {
			return 0;
		}
	}
}

inline int
__dbhTuplesOK(
	const char	*functionName,
	struct dbh	*dbh,
	PGresult	*result)
{
	int status = PQresultStatus(result);
	if (status == PGRES_TUPLES_OK) {
		return 1;
	} else {
		reportError("DB (%s) Cannot execute query: status=%d (%s)",
				__FUNCTION__,
				status,
				PQerrorMessage(dbh->conn));
		return 0;
	}
}

inline int
__dbhCommandOK(
	const char	*functionName,
	struct dbh	*dbh,
	PGresult	*result)
{
	int status = PQresultStatus(result);
	if (status == PGRES_COMMAND_OK) {
		return 1;
	} else {
		reportError("DB (%s) Cannot execute command: status=%d (%s)",
				__FUNCTION__,
				status,
				PQerrorMessage(dbh->conn));
		return 0;
	}
}

inline int
__dbhCorrectNumberOfColumns(
	const char	*functionName,
	PGresult	*result,
	int			expectedNumberOfColumns)
{
	int numberOfColumns = PQnfields(result);
	if (numberOfColumns == expectedNumberOfColumns) {
		return 1;
	} else {
		reportError("DB (%s) Returned %d columns, expected %d",
				__FUNCTION__,
				numberOfColumns,
				expectedNumberOfColumns);
		return 0;
	}
}

inline int
__dbhCorrectNumberOfRows(
	const char	*functionName,
	PGresult	*result,
	int			expectedNumberOfRows)
{
	int numberOfRows = PQntuples(result);
	if (numberOfRows == expectedNumberOfRows) {
		return 1;
	} else {
		reportError("DB (%s) Returned %d rows, expected %d",
				functionName,
				numberOfRows,
				expectedNumberOfRows);
		return 0;
	}
}

inline int
__dbhCorrectColumnType(
	const char	*functionName,
	PGresult	*result,
	int 		columnNumber,
	Oid			expectedColumnType)
{
	int columnType = PQftype(result, columnNumber);
	if (columnType == expectedColumnType) {
		return 1;
	} else {
		reportError("DB (%s) Data OID for column %d is %d, expected %d",
				functionName,
				columnNumber,
				columnType,
				expectedColumnType);
		return 0;
	}
}

inline void
dbhExecute(struct dbh *dbh, const char *query)
{
	if (dbh->result != NULL)
		PQclear(dbh->result);

	dbh->result = PQexecParams(dbh->conn, query,
		dbh->arguments.numberOfArguments,
		dbh->arguments.types,
		dbh->arguments.values,
		dbh->arguments.lengths,
		dbh->arguments.formats,
		1);

	dbh->arguments.numberOfArguments = 0;
}

inline void
dbhPushArgument(struct dbh *dbh, char *value, Oid type, int length, int format)
{
	dbh->arguments.values   [dbh->arguments.numberOfArguments] = value;
	dbh->arguments.types    [dbh->arguments.numberOfArguments] = type;
	dbh->arguments.lengths  [dbh->arguments.numberOfArguments] = (value == NULL) ? 0 : length;
	dbh->arguments.formats  [dbh->arguments.numberOfArguments] = format;
	dbh->arguments.numberOfArguments++;
}

inline void
dbhPushBIGINT(struct dbh *dbh, uint64 *value)
{
	dbh->arguments.values   [dbh->arguments.numberOfArguments] = (char *)value;
	dbh->arguments.types    [dbh->arguments.numberOfArguments] = INT8OID;
	dbh->arguments.lengths  [dbh->arguments.numberOfArguments] = (value == NULL) ? 0 : sizeof(uint64);
	dbh->arguments.formats  [dbh->arguments.numberOfArguments] = 1;
	dbh->arguments.numberOfArguments++;
}

inline void
dbhPushINTEGER(struct dbh *dbh, uint32 *value)
{
	dbh->arguments.values   [dbh->arguments.numberOfArguments] = (char *)value;
	dbh->arguments.types    [dbh->arguments.numberOfArguments] = INT4OID;
	dbh->arguments.lengths  [dbh->arguments.numberOfArguments] = (value == NULL) ? 0 : sizeof(uint32);
	dbh->arguments.formats  [dbh->arguments.numberOfArguments] = 1;
	dbh->arguments.numberOfArguments++;
}

inline void
dbhPushDOUBLE(struct dbh *dbh, double *value)
{
	dbh->arguments.values   [dbh->arguments.numberOfArguments] = (char *)value;
	dbh->arguments.types    [dbh->arguments.numberOfArguments] = FLOAT8OID;
	dbh->arguments.lengths  [dbh->arguments.numberOfArguments] = (value == NULL) ? 0 : sizeof(double);
	dbh->arguments.formats  [dbh->arguments.numberOfArguments] = 1;
	dbh->arguments.numberOfArguments++;
}

inline void
dbhPushREAL(struct dbh *dbh, float *value)
{
	dbh->arguments.values   [dbh->arguments.numberOfArguments] = (char *)value;
	dbh->arguments.types    [dbh->arguments.numberOfArguments] = FLOAT4OID;
	dbh->arguments.lengths  [dbh->arguments.numberOfArguments] = (value == NULL) ? 0 : sizeof(float);
	dbh->arguments.formats  [dbh->arguments.numberOfArguments] = 1;
	dbh->arguments.numberOfArguments++;
}

inline void
dbhPushCHAR(struct dbh *dbh, char *value, int length)
{
	dbh->arguments.values   [dbh->arguments.numberOfArguments] = value;
	dbh->arguments.types    [dbh->arguments.numberOfArguments] = CHAROID;
	dbh->arguments.lengths  [dbh->arguments.numberOfArguments] = (value == NULL) ? 0 : length;
	dbh->arguments.formats  [dbh->arguments.numberOfArguments] = 0;
	dbh->arguments.numberOfArguments++;
}

inline void
dbhPushVARCHAR(struct dbh *dbh, char *value, int length)
{
	dbh->arguments.values   [dbh->arguments.numberOfArguments] = value;
	dbh->arguments.types    [dbh->arguments.numberOfArguments] = VARCHAROID;
	dbh->arguments.lengths  [dbh->arguments.numberOfArguments] = (value == NULL) ? 0 : length;
	dbh->arguments.formats  [dbh->arguments.numberOfArguments] = 0;
	dbh->arguments.numberOfArguments++;
}

inline void
dbhPushBYTEA(struct dbh *dbh, char *value, int length)
{
	dbh->arguments.values   [dbh->arguments.numberOfArguments] = value;
	dbh->arguments.types    [dbh->arguments.numberOfArguments] = BYTEAOID;
	dbh->arguments.lengths  [dbh->arguments.numberOfArguments] = (value == NULL) ? 0 : length;
	dbh->arguments.formats  [dbh->arguments.numberOfArguments] = 1;
	dbh->arguments.numberOfArguments++;
}

inline void
dbhPushUUID(struct dbh *dbh, char *value)
{
	dbh->arguments.values   [dbh->arguments.numberOfArguments] = value;
	dbh->arguments.types    [dbh->arguments.numberOfArguments] = UUIDOID;
	dbh->arguments.lengths  [dbh->arguments.numberOfArguments] = (value == NULL) ? 0 : UUIDBinarySize;
	dbh->arguments.formats  [dbh->arguments.numberOfArguments] = 1;
	dbh->arguments.numberOfArguments++;
}

inline uint64
dbhGetUInt64(PGresult *result, int rowNumber, int columnNumber)
{
	char *c = PQgetvalue(result, rowNumber, columnNumber);
	uint64 value;
	memcpy((void *)&value, c, sizeof(value));
	value = be64toh(value);
	return value;
}

int
dbhInUse(struct dbChain *chain)
{
	pthread_spin_lock(&chain->lock);
	int dbhInUse = (chain->peekCursor >= chain->pokeCursor)
		? chain->peekCursor - chain->pokeCursor
		: chain->numberOfConnections - (chain->pokeCursor - chain->peekCursor);
	pthread_spin_unlock(&chain->lock);

	return dbhInUse;
}
