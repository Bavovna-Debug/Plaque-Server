#include <c.h>
#include <string.h>

#include "api.h"
#include "db.h"
#include "paquet.h"
#include "paquet_broadcast.h"
#include "buffers.h"
#include "report.h"
#include "tasks.h"

static void
rejectPaquetASBusy(struct paquet *paquet);

void *
paquetCleanup(void *arg);

void *
paquetThread(void *arg)
{
	struct paquet *paquet = (struct paquet *)arg;
	struct task *task = paquet->task;
	int rc;

    pthread_cleanup_push(&paquetCleanup, paquet);

	appentPaquetToTask(task, paquet);

	switch (paquet->commandCode)
	{
		case PaquetBroadcastForOnRadar:
			rc = paquetBroadcastForOnRadar(paquet);
			break;

		case PaquetBroadcastForInSight:
			rc = paquetBroadcastForInSight(paquet);
			break;

		case PaquetBroadcastForOnMap:
			rc = paquetBroadcastForOnMap(paquet);
			break;

		case PaquetDisplacementOnRadar:
			rc = paquetDisplacementOnRadar(paquet);
			break;

		case PaquetDisplacementInSight:
			rc = paquetDisplacementInSight(paquet);
			break;

		case PaquetDownloadPlaquesOnRadar:
		case PaquetDownloadPlaquesInSight:
		case PaquetDownloadPlaquesOnMap:
			if (pthread_spin_trylock(&task->paquet.downloadLock) == 0) {
				rc = paquetDownloadPlaques(paquet);
				pthread_spin_unlock(&task->paquet.downloadLock);
			} else {
				rejectPaquetASBusy(paquet);
			}
			break;

		case PaquetPostNewPlaque:
			rc = paquetPostNewPlaque(paquet);
			break;

		case PaquetPlaqueModifiedLocation:
			rc = paquetChangePlaqueLocation(paquet);
			break;

		case PaquetPlaqueModifiedOrientation:
			rc = paquetChangePlaqueOrientation(paquet);
			break;

		case PaquetPlaqueModifiedSize:
			rc = paquetChangePlaqueSize(paquet);
			break;

		case PaquetPlaqueModifiedColors:
			rc = paquetChangePlaqueColors(paquet);
			break;

		case PaquetPlaqueModifiedFont:
			rc = paquetChangePlaqueFont(paquet);
			break;

		case PaquetPlaqueModifiedInscription:
			rc = paquetChangePlaqueInscription(paquet);
			break;

		case PaquetDownloadProfiles:
			rc = getProfiles(paquet);
			break;

		case PaquetNotificationsToken:
			rc = notificationsToken(paquet);
			break;

		case PaquetValidateProfileName:
			rc = validateProfileName(paquet);
			break;

		case PaquetCreateProfile:
			rc = createProfile(paquet);
			break;

		default:
			reportError("Unknown command: commandCode=0x%08X", paquet->commandCode);
			rc = -1;
	}

	if (rc == 0)
		sendPaquet(paquet);

	if (paquet->inputBuffer == paquet->outputBuffer) {
		if (paquet->inputBuffer != NULL)
			pokeBuffer(paquet->inputBuffer);
	} else {
		if (paquet->inputBuffer != NULL)
			pokeBuffer(paquet->inputBuffer);
		if (paquet->outputBuffer != NULL)
			pokeBuffer(paquet->outputBuffer);
	}

    pthread_cleanup_pop(1);

	pthread_exit(NULL);
}

void *
paquetCleanup(void *arg)
{
	struct paquet *paquet = (struct paquet *)arg;
	struct buffer *buffer = paquet->containerBuffer;

	removePaquetFromTask(paquet->task, paquet);

    pokeBuffer(buffer);
}

void
paquetCancel(struct paquet *paquet)
{
	int rc;

	rc = pthread_cancel(paquet->thread);
	if (rc != 0) {
		if (rc == ESRCH) {
	    	reportError("Cannot cancel paquet thread because it is already closed");
	    } else {
	    	reportError("Cannot cancel paquet thread: rc=%d", rc);
	    }
	}
}

int
minimumPayloadSize(struct paquet *paquet, int minimumSize)
{
	if (paquet->payloadSize < minimumSize) {
#ifdef DEBUG
		reportLog("Wrong payload size %d, expected minimum %lu",
			paquet->payloadSize,
			minimumSize);
#endif
		return 0;
	} else {
		return 1;
	}
}

int
expectedPayloadSize(struct paquet *paquet, int expectedSize)
{
	if (paquet->payloadSize != expectedSize) {
#ifdef DEBUG
		reportLog("Wrong payload size %d, expected %lu",
			paquet->payloadSize,
			expectedSize);
#endif
		return 0;
	} else {
		return 1;
	}
}

uint64
deviceIdByToken(struct dbh *dbh, char *deviceToken)
{
	PGresult	*result;
	const char	*paramValues[1];
    Oid			paramTypes[1];
    int			paramLengths[1];
	int			paramFormats[1];

	paramValues   [0] = deviceToken;
	paramTypes    [0] = UUIDOID;
	paramLengths  [0] = TokenBinarySize;
	paramFormats  [0] = 1;

	result = PQexecParams(dbh->conn, "\
SELECT device_id \
FROM auth.devices \
WHERE device_token = $1",
		1, paramTypes, paramValues, paramLengths, paramFormats, 1);

	if (!dbhTuplesOK(dbh, result)) {
		PQclear(result);
		return 0;
	}

	if (PQnfields(result) != 1) {
		PQclear(result);
		return 0;
	}

	if (PQntuples(result) != 1) {
		PQclear(result);
		return 0;
	}

	if (PQftype(result, 0) != INT8OID) {
		PQclear(result);
		return 0;
	}

	uint64 deviceIdBigEndian;
	memcpy(&deviceIdBigEndian, PQgetvalue(result, 0, 0), sizeof(deviceIdBigEndian));

	PQclear(result);

	return deviceIdBigEndian;
}

static void
rejectPaquetASBusy(struct paquet *paquet)
{
	paquet->outputBuffer = paquet->inputBuffer;
	resetBufferData(paquet->outputBuffer, 1);

	struct paquetPilot *pilot = (struct paquetPilot *)paquet->outputBuffer;
	pilot->commandSubcode = PaquetRejectBusy;
	pilot->payloadSize = 0;
}
