#ifndef __PAQUET__
#define __PAQUET__

#include <c.h>

#include "api.h"
#include "db.h"
#include "tasks.h"

#pragma pack(push, 1)
struct PaquetPilot
{
	uint64  		signature;
	uint32  		paquetId;
	uint32  		commandCode;
	uint32  		commandSubcode;
	uint32  		payloadSize;
	char			payload[];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetBroadcast
{
	uint32  		lastKnownOnRadarRevision;
	uint32  		lastKnownInSightRevision;
	uint32  		lastKnownOnMapRevision;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetDisplacement
{
	double			latitude;
	double			longitude;
	float			altitude;
	char			courseAvailable;
	float			course;
	char			floorLevelAvailable;
	uint32  		floorLevel;
	float			range;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetPostPlaque
{
	//char[2]			dimension;
	double			latitude;
	double			longitude;
	float			altitude;
	char			directed;
	float			direction;
	char			tilted;
	float			tilt;
	float			width;
	float			height;
	uint32  		backgroundColor;
	uint32  		foregroundColor;
	float			fontSize;
	uint32  		inscriptionLength;
	char			inscription[];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetPlaqueLocation
{
	char			plaqueToken[API_TokenBinarySize];
	double			latitude;
	double			longitude;
	float			altitude;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetPlaqueOrientation
{
	char			plaqueToken[API_TokenBinarySize];
	char			directed;
	float			direction;
	char			tilted;
	float			tilt;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetPlaqueSize
{
	char			plaqueToken[API_TokenBinarySize];
	float			width;
	float			height;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetPlaqueColors
{
	char			plaqueToken[API_TokenBinarySize];
	uint32  		backgroundColor;
	uint32  		foregroundColor;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetPlaqueFont
{
	char			plaqueToken[API_TokenBinarySize];
	float			fontSize;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetPlaqueInscription
{
	char			plaqueToken[API_TokenBinarySize];
	uint32  		inscriptionLength;
	char			inscription[];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetNotificationsToken
{
	char			notificationsToken[API_NotificationsTokenBinarySize];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PaquetReport
{
	uint32  		messageLength;
	char			message[];
};
#pragma pack(pop)

void *
PaquetThread(void *arg);

void
PaquetCancel(struct Paquet *paquet);

int
MinimumPayloadSize(struct Paquet *paquet, int minimumSize);

int
ExpectedPayloadSize(struct Paquet *paquet, int expectedSize);

uint64
DeviceIdByToken(struct dbh *dbh, char *deviceToken);

#endif
