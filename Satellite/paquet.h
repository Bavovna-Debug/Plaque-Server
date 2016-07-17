#ifndef __PAQUET__
#define __PAQUET__

#include <c.h>

#include "api.h"
#include "db.h"
#include "tasks.h"

#pragma pack(push, 1)
struct paquetPilot
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
struct paquetBroadcast
{
	uint32  		lastKnownOnRadarRevision;
	uint32  		lastKnownInSightRevision;
	uint32  		lastKnownOnMapRevision;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct paquetDisplacement
{
	double			latitude;
	double			longitude;
	float			altitude;
	char			courseAvailable;
	float			course;
	char			floorLevelAvailable;
	int32  			floorLevel;
	float			range;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct paquetPostPlaque
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
struct paquetPlaqueLocation
{
	char			plaqueToken[TokenBinarySize];
	double			latitude;
	double			longitude;
	float			altitude;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct paquetPlaqueOrientation
{
	char			plaqueToken[TokenBinarySize];
	char			directed;
	float			direction;
	char			tilted;
	float			tilt;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct paquetPlaqueSize
{
	char			plaqueToken[TokenBinarySize];
	float			width;
	float			height;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct paquetPlaqueColors
{
	char			plaqueToken[TokenBinarySize];
	uint32  		backgroundColor;
	uint32  		foregroundColor;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct paquetPlaqueFont
{
	char			plaqueToken[TokenBinarySize];
	float			fontSize;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct paquetPlaqueInscription
{
	char			plaqueToken[TokenBinarySize];
	uint32  		inscriptionLength;
	char			inscription[];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct paquetNotificationsToken
{
	char			notificationsToken[NotificationsTokenBinarySize];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct paquetReport
{
	uint32  		messageLength;
	char			message[];
};
#pragma pack(pop)

void *
paquetThread(void *arg);

void
paquetCancel(struct paquet *paquet);

int
minimumPayloadSize(struct paquet *paquet, int minimumSize);

int
expectedPayloadSize(struct paquet *paquet, int expectedSize);

uint64
deviceIdByToken(struct dbh *dbh, char *deviceToken);

#endif
