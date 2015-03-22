#ifndef _ANTICIPANT_
#define _ANTICIPANT_

#include "api.h"
#include "paquet.h"
#include "tasks.h"
#include "db.h"

#pragma pack(push, 1)
typedef struct dialogueAnticipant {
        char                    vendorToken[TokenBinarySize];
        char                    deviceName[AnticipantDeviceNameLength];
        char                    deviceModel[AnticipantDeviceModelLength];
        char                    systemName[AnticipantSystemNamelLength];
        char                    systemVersion[AnticipantSystemVersionlLength];
} bonjourAnticipant;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct bonjourProfileNameValidation {
        char                    profileName[BonjourProfileNameLength];
} bonjourProfileNameValidation;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct bonjourCreateProfile {
        char                    profileName[BonjourProfileNameLength];
        char                    userName[BonjourUserNameLength];
        char                    passwordMD5[BonjourMD5Length];
        char                    emailAddress[BonjourEmailAddressLength];
} bonjourCreateProfile;
#pragma pack(pop)

int verifyGuest(struct task *task);

int registerDevice(struct task *task, struct dialogueAnticipant *anticipant, char *deviceToken);

int validateProfileName(struct paquet *paquet);

int createProfile(struct paquet *paquet);

int notificationsToken(struct paquet *paquet);

#endif
