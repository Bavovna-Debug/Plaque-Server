#ifndef __ANTICIPANT__
#define __ANTICIPANT__

#include "api.h"
#include "db.h"
#include "paquet.h"
#include "tasks.h"

#pragma pack(push, 1)
struct dialogueAnticipant
{
        char                    vendorToken[TokenBinarySize];
        char                    deviceName[AnticipantDeviceNameLength];
        char                    deviceModel[AnticipantDeviceModelLength];
        char                    systemName[AnticipantSystemNamelLength];
        char                    systemVersion[AnticipantSystemVersionlLength];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct bonjourProfileNameValidation
{
        char                    profileName[BonjourProfileNameLength];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct bonjourCreateProfile
{
        char                    profileName[BonjourProfileNameLength];
        char                    userName[BonjourUserNameLength];
        char                    passwordMD5[BonjourMD5Length];
        char                    emailAddress[BonjourEmailAddressLength];
};
#pragma pack(pop)

int
verifyGuest(struct task *task);

int
registerDevice(
	struct task *task,
	struct dialogueAnticipant *anticipant,
	char *deviceToken);

int
validateProfileName(struct paquet *paquet);

int
createProfile(struct paquet *paquet);

int
notificationsToken(struct paquet *paquet);

#endif
