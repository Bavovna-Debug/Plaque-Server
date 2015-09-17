#include <c.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "report.h"

#define PEEK_DBH_RETRIES				5
#define PEEK_DBH_RETRY_SLEEP			0.2

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

		pthread_spin_init(&dbh->lock, PTHREAD_PROCESS_PRIVATE);

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

		pthread_spin_destroy(&dbh->lock);
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
			sleep(PEEK_DBH_RETRY_SLEEP);
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

		int locked = pthread_spin_trylock(&dbh->lock);
		if (locked != 0) {
			reportLog("ROLLBACK for DB %d", dbh->dbhId);

			pthread_spin_unlock(&dbh->lock);

			pthread_spin_lock(&dbh->lock);

			// Rollback all previous transactions.
			//
			PGresult *result = PQexec(dbh->conn, "ROLLBACK");
			PQclear(result);
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
		disconnectFromPostgres(dbh);
	}

	pthread_spin_unlock(&dbh->lock);

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
dbhTuplesOK(struct dbh *dbh, PGresult *result)
{
	int status = PQresultStatus(result);
	if (status == PGRES_TUPLES_OK) {
		return 1;
	} else {
		reportError("Cannot execute query: status=%d (%s)",
				status,
				PQerrorMessage(dbh->conn));
		return 0;
	}
}

inline int
dbhCommandOK(struct dbh *dbh, PGresult *result)
{
	int status = PQresultStatus(result);
	if (status == PGRES_COMMAND_OK) {
		return 1;
	} else {
		reportError("Cannot execute command: status=%d (%s)",
				status,
				PQerrorMessage(dbh->conn));
		return 0;
	}
}

inline int
dbhCorrectNumberOfColumns(PGresult *result, int expectedNumberOfColumns)
{
	int numberOfColumns = PQnfields(result);
	if (numberOfColumns == expectedNumberOfColumns) {
		return 1;
	} else {
		reportError("Returned %d columns, expected %d",
				numberOfColumns,
				expectedNumberOfColumns);
		return 0;
	}
}

inline int
dbhCorrectNumberOfRows(PGresult *result, int expectedNumberOfRows)
{
	int numberOfRows = PQntuples(result);
	if (numberOfRows == expectedNumberOfRows) {
		return 1;
	} else {
		reportError("Returned %d rows, expected %d",
				numberOfRows,
				expectedNumberOfRows);
		return 0;
	}
}

inline int
dbhCorrectColumnType(PGresult *result, int columnNumber, Oid expectedColumnType)
{
	int columnType = PQftype(result, columnNumber);
	if (columnType == expectedColumnType) {
		return 1;
	} else {
		reportError("Data OID for column %d is %d, expected %d",
				columnNumber,
				columnType,
				expectedColumnType);
		return 0;
	}
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
