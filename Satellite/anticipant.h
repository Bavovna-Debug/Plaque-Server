#ifndef __ANTICIPANT__
#define __ANTICIPANT__

#include "api.h"
#include "db.h"
#include "paquet.h"
#include "tasks.h"

#pragma pack(push, 1)
struct DialogueAnticipant
{
        char                    vendorToken[TokenBinarySize];
        char                    deviceName[AnticipantDeviceNameLength];
        char                    deviceModel[AnticipantDeviceModelLength];
        char                    systemName[AnticipantSystemNamelLength];
        char                    systemVersion[AnticipantSystemVersionlLength];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BonjourProfileNameValidation
{
        char                    profileName[BonjourProfileNameLength];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BonjourCreateProfile
{
        char                    profileName[BonjourProfileNameLength];
        char                    userName[BonjourUserNameLength];
        char                    passwordMD5[BonjourMD5Length];
        char                    emailAddress[BonjourEmailAddressLength];
};
#pragma pack(pop)

/**
 * VerifyGuest()
 *
 * @task:
 */
int
VerifyGuest(struct task *task);

/**
 * RegisterDevice()
 *
 * @task:
 * @anticipant:
 * @deviceToken:
 */
int
RegisterDevice(
	struct task 				*task,
	struct DialogueAnticipant 	*anticipant,
	char 						*deviceToken);

/**
 * ValidateProfileName()
 *
 * @paquet:
 */
int
ValidateProfileName(struct paquet *paquet);

/**
 * CreateProfile()
 *
 * @paquet:
 */
int
CreateProfile(struct paquet *paquet);

/**
 * NotificationsToken()
 *
 * @paquet:
 */
int
NotificationsToken(struct paquet *paquet);

#endif
