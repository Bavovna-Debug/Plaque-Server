#ifndef _BONJOUR_
#define _BONJOUR_

#include <netinet/in.h>
#include "db.h"
#include "buffers.h"
#include "tasks.h"

#define BONJOUR_ID			0x00C2D6D5D1D6E4D9
#define BONSOIR_ID			0x00C2D6D5E2D6C9D9

#define TOKEN_SIZE			16

#pragma pack(push, 1)
typedef struct bonjourPilot {
	uint64_t		projectId;
	char			deviceToken[TOKEN_SIZE];
	uint32_t		commandCode;
	uint32_t		payloadSize;
	char			payload[];
} bonjourPilot;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct bonjourRadar {
	double			latitude;
	double			longitude;
	float			range;
} bonjourRadar;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct bonjourGetPlaquesHeader {
	uint32_t		numberOfPlaques;
} bonjourGetPlaquesHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct bonjourGetPlaquesElement {
	char			plaqueToken[TOKEN_SIZE];
} bonjourGetPlaquesElement;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct bonjourNewPlaque {
	double			latitude;
	double			longitude;
	float			altitude;
	char			directed;
	float			direction;
	float			width;
	float			height;
	uint32_t		backgroundColor;
	uint32_t		foregroundColor;
	uint32_t		inscriptionLength;
	char			inscription[];
} bonjourNewPlaque;
#pragma pack(pop)

void bonjourCopyPilot(struct buffer *destination, struct buffer *source);

char *bonjourDeviceToken(struct buffer *buffer);

uint32_t bonjourGetCommandCode(struct buffer *buffer);

uint32_t bonjourGetPayloadSize(struct buffer *buffer);

void bonjourSetPayloadSize(struct buffer *buffer, uint32_t payloadSize);

int processBonjour(struct task *task);

uint64_t deviceIdByToken(struct dbh *dbh, char *deviceToken);

#endif
