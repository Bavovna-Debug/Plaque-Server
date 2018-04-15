#pragma once

#include "api.h"
#include "db.h"
#include "paquet.h"
#include "tasks.h"

#pragma pack(push, 1)
struct DialogueAnticipant
{
        char                    vendorToken[API_TokenBinarySize];
        char                    deviceName[API_AnticipantDeviceNameLength];
        char                    deviceModel[API_AnticipantDeviceModelLength];
        char                    systemName[API_AnticipantSystemNamelLength];
        char                    systemVersion[API_AnticipantSystemVersionlLength];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BonjourProfileNameValidation
{
        char                    profileName[API_BonjourProfileNameLength];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BonjourCreateProfile
{
        char                    profileName[API_BonjourProfileNameLength];
        char                    userName[API_BonjourUserNameLength];
        char                    passwordMD5[API_BonjourMD5Length];
        char                    emailAddress[API_BonjourEmailAddressLength];
};
#pragma pack(pop)

/**
 * VerifyGuest()
 *
 * @task:
 */
int
VerifyGuest(struct Task *task);

/**
 * RegisterDevice()
 *
 * @task:
 * @anticipant:
 * @deviceToken:
 */
int
RegisterDevice(
	struct Task 				*task,
	struct DialogueAnticipant 	*anticipant,
	char 						*deviceToken);

/**
 * ValidateProfileName()
 *
 * @paquet:
 */
int
ValidateProfileName(struct Paquet *paquet);

/**
 * CreateProfile()
 *
 * @paquet:
 */
int
CreateProfile(struct Paquet *paquet);

/**
 * NotificationsToken()
 *
 * @paquet:
 */
int
NotificationsToken(struct Paquet *paquet);
