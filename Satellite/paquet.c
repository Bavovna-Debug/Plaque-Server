#include <string.h>
#include "api.h"
#include "db.h"
#include "paquet.h"
#include "buffers.h"
#include "tasks.h"

void rejectPaquetASBusy(struct paquet *paquet)
{
	paquet->outputBuffer = paquet->inputBuffer;
	resetBufferData(paquet->outputBuffer, 1);

	struct paquetPilot *pilot = (struct paquetPilot *)paquet->outputBuffer;
	pilot->commandSubcode = PaquetRejectBusy;
	pilot->payloadSize = 0;
}

int minimumPayloadSize(struct paquet *paquet, int minimumSize)
{
	if (paquet->payloadSize < minimumSize) {
#ifdef DEBUG
		fprintf(stderr, "Wrong payload size %d, expected minimum %lu\n",
			paquet->payloadSize,
			minimumSize);
#endif
		return 0;
	} else {
		return 1;
	}
}

int expectedPayloadSize(struct paquet *paquet, int expectedSize)
{
	if (paquet->payloadSize != expectedSize) {
#ifdef DEBUG
		fprintf(stderr, "Wrong payload size %d, expected %lu\n",
			paquet->payloadSize,
			expectedSize);
#endif
		return 0;
	} else {
		return 1;
	}
}

uint64_t deviceIdByToken(struct dbh *dbh, char *deviceToken)
{
	PGresult	*result;
	const char*	paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	paramValues   [0] = deviceToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = TokenBinarySize;
	paramFormats  [0] = 1;

	result = PQexecParams(dbh->conn, "SELECT device_id FROM auth.devices WHERE device_token = $1",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		return 0;
	}

	if ((PQnfields(result) != 1) || (PQntuples(result) != 1) || (PQftype(result, 0) != INT8OID)) {
		PQclear(result);
		return 0;
	}

	uint64_t deviceIdBigEndian;
	memcpy(&deviceIdBigEndian, PQgetvalue(result, 0, 0), sizeof(deviceIdBigEndian));

	PQclear(result);

	return deviceIdBigEndian;
}

uint64_t profileIdByToken(struct dbh *dbh, char *profileToken)
{
	PGresult	*result;
	const char*	paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	paramValues   [0] = profileToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = TokenBinarySize;
	paramFormats  [0] = 1;

	result = PQexecParams(dbh->conn, "SELECT profile_id FROM auth.profiles WHERE profile_token = $1",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		return 0;
	}

	if ((PQnfields(result) != 1) || (PQntuples(result) != 1) || (PQftype(result, 0) != INT8OID)) {
		PQclear(result);
		return 0;
	}

	uint64_t profileIdBigEndian;
	memcpy(&profileIdBigEndian, PQgetvalue(result, 0, 0), sizeof(profileIdBigEndian));

	PQclear(result);

	return profileIdBigEndian;
}
