#include <c.h>
#include <string.h>

#include "anticipant.h"
#include "api.h"
#include "db.h"
#include "mmps.h"
#include "paquet.h"
#include "paquet_broadcast.h"
#include "paquet_displacement.h"
#include "plaques_edit.h"
#include "plaques_query.h"
#include "profiles.h"
#include "report.h"
#include "reports.h"
#include "tasks.h"
#include "task_xmit.h"

static void
RejectPaquetAsBusy(struct Paquet *paquet);

static void
RejectPaquetAsError(struct Paquet *paquet);

void
PaquetCleanup(void *arg);

void *
PaquetThread(void *arg)
{
	struct Paquet *paquet = (struct Paquet *)arg;
	struct Task *task = paquet->task;
	int rc;

    pthread_cleanup_push(&PaquetCleanup, paquet);

	AppentPaquetToTask(task, paquet);

	switch (paquet->commandCode)
	{
		case API_PaquetBroadcast:
			rc = HandleBroadcast(paquet);
			break;

		case API_PaquetDisplacementOnRadar:
			rc = HandleDisplacementOnRadar(paquet);
			break;

		case API_PaquetDisplacementInSight:
			rc = HandleDisplacementInSight(paquet);
			break;

		case API_PaquetDisplacementOnMap:
			rc = HandleDisplacementOnMap(paquet);
			break;

		case API_PaquetDownloadPlaquesOnRadar:
		case API_PaquetDownloadPlaquesInSight:
		case API_PaquetDownloadPlaquesOnMap:
			if (pthread_mutex_trylock(&task->paquet.downloadMutex) == 0) {
				rc = HandleDownloadPlaques(paquet);
				pthread_mutex_unlock(&task->paquet.downloadMutex);
			} else {
				RejectPaquetAsBusy(paquet);
			}
			break;

		case API_PaquetPostNewPlaque:
			rc = HandlePostNewPlaque(paquet);
			break;

		case API_PaquetPlaqueModifiedLocation:
			rc = HandleChangePlaqueLocation(paquet);
			break;

		case API_PaquetPlaqueModifiedOrientation:
			rc = HandleChangePlaqueOrientation(paquet);
			break;

		case API_PaquetPlaqueModifiedSize:
			rc = HandleChangePlaqueSize(paquet);
			break;

		case API_PaquetPlaqueModifiedColors:
			rc = HandleChangePlaqueColors(paquet);
			break;

		case API_PaquetPlaqueModifiedFont:
			rc = HandleChangePlaqueFont(paquet);
			break;

		case API_PaquetPlaqueModifiedInscription:
			rc = HandleChangePlaqueInscription(paquet);
			break;

		case API_PaquetDownloadProfiles:
			rc = GetProfiles(paquet);
			break;

		case API_PaquetNotificationsToken:
			rc = NotificationsToken(paquet);
			break;

		case API_PaquetValidateProfileName:
			rc = ValidateProfileName(paquet);
			break;

		case API_PaquetCreateProfile:
			rc = CreateProfile(paquet);
			break;

		case API_PaquetReportMessage:
			rc = ReportMessage(paquet);
			break;

		default:
			ReportError("Unknown command: commandCode=0x%08X", paquet->commandCode);
			rc = -1;
	}

	if (rc != 0)
		RejectPaquetAsError(paquet);

	SendPaquet(paquet);

    pthread_cleanup_pop(1);

	pthread_exit(NULL);
}

void
PaquetCleanup(void *arg)
{
	struct Paquet *paquet = (struct Paquet *)arg;

	RemovePaquetFromTask(paquet->task, paquet);

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
PaquetCancel(struct Paquet *paquet)
{
	int rc;

	rc = pthread_cancel(paquet->thread);
	if (rc != 0) {
		if (rc == ESRCH) {
	    	ReportError("Cannot cancel paquet thread because it is already closed");
	    } else {
	    	ReportError("Cannot cancel paquet thread: rc=%d", rc);
	    }
	} else {
	    ReportError("Paquet thread cancelled");
	}
}

int
MinimumPayloadSize(struct Paquet *paquet, int minimumSize)
{
	if (paquet->payloadSize < minimumSize) {
#ifdef DEBUG
		ReportInfo("Wrong payload size %d, expected minimum %lu",
			paquet->payloadSize,
			minimumSize);
#endif
		return 0;
	} else {
		return 1;
	}
}

int
ExpectedPayloadSize(struct Paquet *paquet, int expectedSize)
{
	if (paquet->payloadSize != expectedSize) {
#ifdef DEBUG
		ReportInfo("Wrong payload size %d, expected %lu",
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
DeviceIdByToken(struct dbh *dbh, char *deviceToken)
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
RejectPaquetAsBusy(struct Paquet *paquet)
{
	if (paquet->outputBuffer == NULL)
		paquet->outputBuffer = paquet->inputBuffer;

	MMPS_ResetBufferData(paquet->outputBuffer, 1);

	struct PaquetPilot *pilot = (struct PaquetPilot *) paquet->outputBuffer;
	pilot->commandSubcode = API_PaquetRejectBusy;
	pilot->payloadSize = 0;
}

static void
RejectPaquetAsError(struct Paquet *paquet)
{
	if (paquet->outputBuffer == NULL)
		paquet->outputBuffer = paquet->inputBuffer;

	MMPS_ResetBufferData(paquet->outputBuffer, 1);

	struct PaquetPilot *pilot = (struct PaquetPilot *) paquet->outputBuffer;
	pilot->commandSubcode = API_PaquetRejectError;
	pilot->payloadSize = 0;
}
