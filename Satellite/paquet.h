#ifndef _BONJOUR_
#define _BONJOUR_

#include <netinet/in.h>
#include "api.h"
#include "db.h"
#include "buffers.h"
#include "tasks.h"

#pragma pack(push, 1)
typedef struct dialogueDemande {
	uint64_t		dialogueSignature;
	double			deviceTimestamp;
	uint32_t		dialogueType;
	uint8_t			applicationVersion;
	uint8_t			applicationSubersion;
	uint16_t		applicationRelease;
	uint16_t		deviceType;
	char			applicationBuild[6];
	char			deviceToken[TokenBinarySize];
	char			profileToken[TokenBinarySize];
	char			sessionToken[TokenBinarySize];
} dialogueDemande;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct dialogueVerdict {
	uint64_t		dialogueSignature;
	uint32_t		verdictCode;
	char			sessionToken[TokenBinarySize];
} dialogueVerdict;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetPilot {
	uint64_t		signature;
	uint32_t		paquetId;
	uint32_t		commandCode;
	uint32_t		commandSubcode;
	uint32_t		payloadSize;
	char			payload[];
} paquetPilot;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetRadar {
	uint32_t		inSightRevision;
	double			latitude;
	double			longitude;
	float			altitude;
	char			courseAvailable;
	float			course;
	char			floorLevelAvailable;
	int32_t			floorLevel;
	float			range;
} paquetRadar;
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
	uint32_t		backgroundColor;
	uint32_t		foregroundColor;
	float			fontSize;
	uint32_t		inscriptionLength;
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
	uint32_t		backgroundColor;
	uint32_t		foregroundColor;
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
	uint32_t		inscriptionLength;
	char			inscription[];
} paquetPlaqueInscription;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct paquetNotificationsToken {
	char			notificationsToken[NotificationsTokenBinarySize];
} paquetNotificationsToken;
#pragma pack(pop)

void rejectPaquetASBusy(struct paquet *paquet);

int minimumPayloadSize(struct paquet *paquet, int minimumSize);

int expectedPayloadSize(struct paquet *paquet, int expectedSize);

uint64_t deviceIdByToken(struct dbh *dbh, char *deviceToken);

uint64_t profileIdByToken(struct dbh *dbh, char *profileToken);

#endif
