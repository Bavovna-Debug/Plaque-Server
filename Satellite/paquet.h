#ifndef _PAQUET_
#define _PAQUET_

#include <c.h>

#include "api.h"
#include "db.h"
#include "tasks.h"

#pragma pack(push, 1)
typedef struct dialogueDemande {
	uint64  		dialogueSignature;
	double			deviceTimestamp;
	uint32  		dialogueType;
	uint8  			applicationVersion;
	uint8  			applicationSubersion;
	uint16  		applicationRelease;
	uint16  		deviceType;
	char			applicationBuild[6];
	char			deviceToken[TokenBinarySize];
	char			profileToken[TokenBinarySize];
	char			sessionToken[TokenBinarySize];
} dialogueDemande;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct dialogueVerdict {
	uint64  		dialogueSignature;
	uint32  		verdictCode;
	char			sessionToken[TokenBinarySize];
} dialogueVerdict;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetPilot {
	uint64  		signature;
	uint32  		paquetId;
	uint32  		commandCode;
	uint32  		commandSubcode;
	uint32  		payloadSize;
	char			payload[];
} paquetPilot;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetBroadcast {
	uint32  		lastKnownOnRadarRevision;
	uint32  		lastKnownInSightRevision;
	uint32  		lastKnownOnMapRevision;
} paquetRadar;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetDisplacement {
	double			latitude;
	double			longitude;
	float			altitude;
	char			courseAvailable;
	float			course;
	char			floorLevelAvailable;
	int32  			floorLevel;
	float			range;
} paquetDisplacement;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetPostPlaque {
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
} paquetPostPlaque;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetPlaqueLocation {
	char			plaqueToken[TokenBinarySize];
	double			latitude;
	double			longitude;
	float			altitude;
} paquetPlaqueLocation;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetPlaqueOrientation {
	char			plaqueToken[TokenBinarySize];
	char			directed;
	float			direction;
	char			tilted;
	float			tilt;
} paquetPlaqueOrientation;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetPlaqueSize {
	char			plaqueToken[TokenBinarySize];
	float			width;
	float			height;
} paquetPlaqueSize;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetPlaqueColors {
	char			plaqueToken[TokenBinarySize];
	uint32  		backgroundColor;
	uint32  		foregroundColor;
} paquetPlaqueColors;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetPlaqueFont {
	char			plaqueToken[TokenBinarySize];
	float			fontSize;
} paquetPlaqueFont;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetPlaqueInscription {
	char			plaqueToken[TokenBinarySize];
	uint32  		inscriptionLength;
	char			inscription[];
} paquetPlaqueInscription;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetNotificationsToken {
	char			notificationsToken[NotificationsTokenBinarySize];
} paquetNotificationsToken;
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
