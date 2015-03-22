#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "db.h"

#define PEEK_DBH_RETRIES				5
#define PEEK_DBH_RETRY_SLEEP			0.2

#define NUMBER_OF_DBH_GUARDIANS			10//2000
#define NUMBER_OF_DBH_AUTHENTICATION	10//2000
#define NUMBER_OF_DBH_PLAQUES_SESSION	10//4000
#define NOTHING							-1

typedef struct chain {
	pthread_spinlock_t	lock;
	int					numberOfConnections;
	int					peekCursor;
	int					pokeCursor;
	void				*block;
	int					ids[];
} chain_t;

static struct chain *chains[3];

static struct chain *dbhGuardianChain;

int connectToPostgres(struct dbh *dbh, char *conninfo)
{
	pthread_spin_init(&dbh->lock, PTHREAD_PROCESS_PRIVATE);

	// Connect to database.
	//
	//dbh->conn = PQsetdbLogin(PLAQUE_DB_HOST, PLAQUE_DB_PORT, NULL, NULL, PLAQUE_DB_DATABASE, PLAQUE_DB_USERNAME, PLAQUE_DB_PASSWORT);
	dbh->conn = PQconnectdb(conninfo);

	// Check to see that the backend connection was successfully made.
	//
	if (PQstatus(dbh->conn) != CONNECTION_OK) {
		fprintf(stderr, "Connection to database failed: %s", PQerrorMessage(dbh->conn));
		return -1;
	}

	return 0;
}

void disconnectFromPostgres(struct dbh *dbh)
{
	// Disconnect from database and cleanup.
	//
	PQfinish(dbh->conn);

	pthread_spin_destroy(&dbh->lock);
}

void constructDBChain(int chainId, int numberOfConnections, char *conninfo)
{
	struct chain *chain = malloc(sizeof(struct chain) + numberOfConnections * sizeof(int));
	if (chain == NULL) {
        fprintf(stderr, "No memory\n");
        return;
    }

	chain->block = malloc(numberOfConnections * sizeof(struct dbh));
	if (chain->block == NULL) {
		fprintf(stderr, "No memory\n");
		return;
	}

	pthread_spin_init(&chain->lock, PTHREAD_PROCESS_PRIVATE);

	chain->numberOfConnections = numberOfConnections;
	chain->peekCursor = 0;
	chain->pokeCursor = 0;

	int dbhId;
	for (dbhId = 0; dbhId < chain->numberOfConnections; dbhId++)
	{
		struct dbh *dbh = chain->block + dbhId * sizeof(struct dbh);

		dbh->chainId = chainId;

		dbh->dbhId = dbhId;

		if (connectToPostgres(dbh, conninfo) == 0)
			printf("Connected to DB - %d\n", dbhId);
		else
			printf("Cannot connect to DB\n");
	}

	for (dbhId = 0; dbhId < chain->numberOfConnections; dbhId++)
		chain->ids[dbhId] = dbhId;

	chains[chainId] = chain;
}

void destructDBChain(int chainId)
{
	struct chain *chain = chains[chainId];
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

void constructDB(void)
{
	constructDBChain(DB_GUARDIAN, NUMBER_OF_DBH_GUARDIANS,
		"hostaddr = '127.0.0.1' dbname = 'guardian' user = 'guardian' password = 'nVUcDYDVZCMaRdCfayWrG23w'");

	constructDBChain(DB_AUTHENTICATION, NUMBER_OF_DBH_AUTHENTICATION,
		"hostaddr = '127.0.0.1' dbname = 'vp' user = 'vp' password = 'vi79HRhxbFahmCKFUKMAACrY'");

	constructDBChain(DB_PLAQUES_SESSION, NUMBER_OF_DBH_PLAQUES_SESSION,
		"hostaddr = '127.0.0.1' dbname = 'vp' user = 'vp' password = 'vi79HRhxbFahmCKFUKMAACrY'");
}

void destructDB(void)
{
	destructDBChain(DB_GUARDIAN);
	destructDBChain(DB_AUTHENTICATION);
	destructDBChain(DB_PLAQUES_SESSION);
}

struct dbh* peekDB(int chainId)
{
	struct chain *chain = chains[chainId];

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
		int locked = pthread_spin_trylock(&dbh->lock);
		if (locked != 0) {
			printf("ROLLBACK %d (errno: %d)\n", dbh->dbhId, locked);

			pthread_spin_unlock(&dbh->lock);

			pthread_spin_lock(&dbh->lock);

			// Rollback all previous transactions.
			//
			PGresult *result = PQexec(dbh->conn, "ROLLBACK");
			PQclear(result);
		}

		// Start the transaction block.
		//
		PGresult *result = PQexec(dbh->conn, "BEGIN");
		if (PQresultStatus(result) != PGRES_COMMAND_OK) {
			pokeDB(dbh);
			fprintf(stderr, "Start transaction failed: %s", PQerrorMessage(dbh->conn));
			dbh = NULL;
		}

		PQclear(result);
	}

#ifdef DEBUG
	if (dbh == NULL)
		fprintf(stderr, "No database handler available\n");
#endif

	return dbh;
}

void pokeDB(struct dbh* dbh)
{
	struct chain *chain = chains[dbh->chainId];

	// Commit transaction.
	//
	PGresult *result = PQexec(dbh->conn, "COMMIT");
	PQclear(result);

	pthread_spin_unlock(&dbh->lock);

	pthread_spin_lock(&chain->lock);

	chain->ids[chain->pokeCursor] = dbh->dbhId;

	chain->pokeCursor++;
	if (chain->pokeCursor == chain->numberOfConnections)
		chain->pokeCursor = 0;

	pthread_spin_unlock(&chain->lock);
}

void resetDB(struct dbh* dbh)
{
	// Rollback all previous transactions.
	//
	PGresult *result = PQexec(dbh->conn, "ROLLBACK");
	PQclear(result);

	PQreset(dbh->conn);
}

inline int sqlState(PGresult *result, const char *checkState)
{
	char *sqlStateString = PQresultErrorField(result, PG_DIAG_SQLSTATE);
	if (sqlStateString == NULL) {
		return 0;
	} else {
		//printf("%s == %s -> %d\n", sqlStateString, checkState, (strncmp(sqlStateString, checkState, 5) == 0) ? 1 : 0);
		if (strncmp(sqlStateString, checkState, 5) == 0) {
			return 1;
		} else {
			return 0;
		}
	}
}

inline int dbhTuplesOK(struct dbh *dbh, PGresult *result)
{
	int status = PQresultStatus(result);
	if (status == PGRES_TUPLES_OK) {
		return 1;
	} else {
		fprintf(stderr, "Cannot execute query. Status: %d (%s)\n", status, PQerrorMessage(dbh->conn));
		return 0;
	}
}

inline int dbhCommandOK(struct dbh *dbh, PGresult *result)
{
	int status = PQresultStatus(result);
	if (status == PGRES_COMMAND_OK) {
		return 1;
	} else {
		fprintf(stderr, "Cannot execute command. Status: %d (%s)\n", status, PQerrorMessage(dbh->conn));
		return 0;
	}
}

inline int dbhCorrectNumberOfColumns(PGresult *result, int expectedNumberOfColumns)
{
	int numberOfColumns = PQnfields(result);
	if (numberOfColumns == expectedNumberOfColumns) {
		return 1;
	} else {
		fprintf(stderr, "Returned %d columns, expected %d\n", numberOfColumns, expectedNumberOfColumns);
		return 0;
	}
}

inline int dbhCorrectNumberOfRows(PGresult *result, int expectedNumberOfRows)
{
	int numberOfRows = PQntuples(result);
	if (numberOfRows == expectedNumberOfRows) {
		return 1;
	} else {
		fprintf(stderr, "Returned %d rows, expected %d\n", numberOfRows, expectedNumberOfRows);
		return 0;
	}
}

inline int dbhCorrectColumnType(PGresult *result, int columnNumber, Oid expectedColumnType)
{
	int columnType = PQftype(result, columnNumber);
	if (columnType == expectedColumnType) {
		return 1;
	} else {
		fprintf(stderr, "Data OID for column %d is %d, expected %d\n", columnNumber, columnType, expectedColumnType);
		return 0;
	}
}

inline uint64_t dbhGetUInt64(PGresult *result, int rowNumber, int columnNumber)
{
	char *c = PQgetvalue(result, rowNumber, columnNumber);
	uint64_t value;
	memcpy((void *)&value, c, sizeof(value));
	value = be64toh(value);
	return value;
}

int dbhInUse(void)
{
	struct chain *chain = chains[0];

	pthread_spin_lock(&chain->lock);
	int dbhInUse = (chain->peekCursor >= chain->pokeCursor)
		? chain->peekCursor - chain->pokeCursor
		: chain->numberOfConnections - (chain->pokeCursor - chain->peekCursor);
	pthread_spin_unlock(&chain->lock);

	return dbhInUse;
}
