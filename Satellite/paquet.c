#include <c.h>
#include <string.h>

#include "api.h"
#include "db.h"
#include "mmps.h"
#include "paquet.h"
#include "paquet_broadcast.h"
#include "paquet_displacement.h"
#include "plaques_edit.h"
#include "plaques_query.h"
#include "report.h"
#include "reports.h"
#include "tasks.h"

static void
rejectPaquetAsBusy(struct paquet *paquet);

static void
rejectPaquetAsError(struct paquet *paquet);

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
		case PaquetBroadcast:
			rc = paquetBroadcast(paquet);
			break;

		case PaquetDisplacementOnRadar:
			rc = paquetDisplacementOnRadar(paquet);
			break;

		case PaquetDisplacementInSight:
			rc = paquetDisplacementInSight(paquet);
			break;

		case PaquetDisplacementOnMap:
			rc = paquetDisplacementOnMap(paquet);
			break;

		case PaquetDownloadPlaquesOnRadar:
		case PaquetDownloadPlaquesInSight:
		case PaquetDownloadPlaquesOnMap:
			if (pthread_mutex_trylock(&task->paquet.downloadMutex) == 0) {
				rc = paquetDownloadPlaques(paquet);
				pthread_mutex_unlock(&task->paquet.downloadMutex);
			} else {
				rejectPaquetAsBusy(paquet);
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

		case PaquetReportMessage:
			rc = reportMessage(paquet);
			break;

		default:
			reportError("Unknown command: commandCode=0x%08X", paquet->commandCode);
			rc = -1;
	}

	if (rc != 0)
		rejectPaquetAsError(paquet);

	sendPaquet(paquet);

    pthread_cleanup_pop(1);

	pthread_exit(NULL);
}

void *
paquetCleanup(void *arg)
{
	struct paquet *paquet = (struct paquet *)arg;

	removePaquetFromTask(paquet->task, paquet);

	if (paquet->inputBuffer == paquet->outputBuffer) {
		if (paquet->inputBuffer != NULL)
			MMPS_PokeBuffer(paquet->inputBuffer);
	} else {
		if (paquet->inputBuffer != NULL)
			MMPS_PokeBuffer(paquet->inputBuffer);
		if (paquet->outputBuffer != NULL)
			MMPS_PokeBuffer(paquet->outputBuffer);
	}

    MMPS_PokeBuffer(paquet->containerBuffer);
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
	} else {
	    reportError("Paquet thread cancelled");
	}
}

int
minimumPayloadSize(struct paquet *paquet, int minimumSize)
{
	if (paquet->payloadSize < minimumSize) {
#ifdef DEBUG
		reportInfo("Wrong payload size %d, expected minimum %lu",
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
		reportInfo("Wrong payload size %d, expected %lu",
			paquet->payloadSize,
			expectedSize);
#endif
		return 0;
	} else {
		return 1;
	}
}

#define QUERY_DEVICE_ID_BY_TOKEN "\
SELECT device_id \
FROM auth.devices \
WHERE device_token = $1"

uint64
deviceIdByToken(struct dbh *dbh, char *deviceToken)
{
    DB_PushUUID(dbh, deviceToken);

	DB_Execute(dbh, QUERY_DEVICE_ID_BY_TOKEN);

	if (!DB_TuplesOK(dbh, dbh->result))
		return 0;

	if (PQnfields(dbh->result) != 1)
		return 0;

	if (PQntuples(dbh->result) != 1)
		return 0;

	if (PQftype(dbh->result, 0) != INT8OID)
		return 0;

	uint64 deviceIdBigEndian;
	memcpy(&deviceIdBigEndian, PQgetvalue(dbh->result, 0, 0), sizeof(deviceIdBigEndian));

	return deviceIdBigEndian;
}

static void
rejectPaquetAsBusy(struct paquet *paquet)
{
	if (paquet->outputBuffer == NULL)
		paquet->outputBuffer = paquet->inputBuffer;

	MMPS_ResetBufferData(paquet->outputBuffer, 1);

	struct paquetPilot *pilot = (struct paquetPilot *)paquet->outputBuffer;
	pilot->commandSubcode = PaquetRejectBusy;
	pilot->payloadSize = 0;
}

static void
rejectPaquetAsError(struct paquet *paquet)
{
	if (paquet->outputBuffer == NULL)
		paquet->outputBuffer = paquet->inputBuffer;

	MMPS_ResetBufferData(paquet->outputBuffer, 1);

	struct paquetPilot *pilot = (struct paquetPilot *)paquet->outputBuffer;
	pilot->commandSubcode = PaquetRejectError;
	pilot->payloadSize = 0;
}
