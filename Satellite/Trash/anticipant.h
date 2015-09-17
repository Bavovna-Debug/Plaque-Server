#ifndef _ANTICIPANT_
#define _ANTICIPANT_

#include "api.h"
#include "bonjour.h"
#include "tasks.h"
#include "db.h"

#pragma pack(push, 1)
typedef struct bonjourAnticipant {
        char                    vendorToken[TOKEN_SIZE];
        char                    deviceName[BonjourDeviceNameLength];
        char                    deviceModel[BonjourDeviceModelLength];
        char                    systemName[BonjourSystemNamelLength];
        char                    systemVersion[BonjourSystemVersionlLength];
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

int registerDevice(struct dbh *dbh, struct task *task);

int validateProfileName(struct dbh *dbh, struct task *task);

int createProfile(struct dbh *dbh, struct task *task);

#endif
