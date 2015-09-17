#include <string.h>
#include "api.h"
#include "db.h"
#include "bonjour.h"
#include "buffers.h"
#include "tasks.h"

void bonjourCopyPilot(struct buffer *destination, struct buffer *source)
{
	memcpy(destination->data, source->data, sizeof(struct bonjourPilot) - sizeof(uint32_t));
}

char *bonjourDeviceToken(struct buffer *buffer)
{
	struct bonjourPilot *pilot = (struct bonjourPilot *)buffer->data;
	return (char *)&pilot->deviceToken;
}

uint32_t bonjourGetCommandCode(struct buffer *buffer)
{
	struct bonjourPilot *pilot = (struct bonjourPilot *)buffer->data;
	uint32_t commandCode = be32toh(pilot->commandCode);
	return commandCode;
}

uint32_t bonjourGetPayloadSize(struct buffer *buffer)
{
	struct bonjourPilot *pilot = (struct bonjourPilot *)buffer->data;
	uint32_t payloadSize = be32toh(pilot->payloadSize);
	return payloadSize;
}

void bonjourSetPayloadSize(struct buffer *buffer, uint32_t payloadSize)
{
	struct bonjourPilot *pilot = (struct bonjourPilot *)buffer->data;
	pilot->payloadSize = htobe32(payloadSize);
}

int processBonjour(struct task *task)
{
	int rc;

	struct dbh *dbh = peekDB(DB_CHAIN_PLAQUES);
	if (dbh == NULL) {
		fprintf(stderr, "No database handler available\n");
		return -1;
	}

	uint32_t commandCode = bonjourGetCommandCode(task->request);
	switch (commandCode)
	{
		case CommandAnticipant:
			rc = registerDevice(dbh, task);
			break;

		case CommandRadar:
			rc = getPlaquesForRadar(dbh, task);
			break;

		case CommandDownloadPlaques:
			rc = getPlaquesWithDetails(dbh, task);
			break;

		case CommandCreatePlaque:
			rc = createPlaque(dbh, task);
			break;

//CommandPlaqueModifiedLocation:
//CommandPlaqueModifiedDirection:
//CommandPlaqueModifiedSize:
//CommandPlaqueModifiedColors:
//CommandPlaqueModifiedInscription:

		case BonjourValidateProfileName:
			rc = validateProfileName(dbh, task);
			break;

		case BonjourCreateProfile:
			rc = createProfile(dbh, task);
			break;

		default:
			printf("Unknown command: %d\n", commandCode);
			rc = -1;
	}

	pokeDB(dbh);

	return rc;
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
	paramLengths  [0] = TOKEN_SIZE;
	paramFormats  [0] = 1;

	result = PQexecParams(dbh->conn, "SELECT device_id FROM auth.devices WHERE device_token = $1",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result))
		return -1;

	uint64_t deviceIdBigEndian;
	if ((PQnfields(result) == 1) && (PQntuples(result) == 1) && (PQftype(result, 0) == INT8OID)) {
		memcpy(&deviceIdBigEndian, PQgetvalue(result, 0, 0), sizeof(deviceIdBigEndian));
	} else {
		deviceIdBigEndian = 0;
	}

	PQclear(result);

	return deviceIdBigEndian;
}
