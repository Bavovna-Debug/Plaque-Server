#pragma once

#define MAX_NOTIFICATIONS           5
#define DEVICE_TOKEN_SIZE           32
#define MESSAGE_KEY_SIZE            64
#define MESSAGE_ARGUMENTS_SIZE      1024

struct Notification {
    uint64              notificationId;
	uint64              deviceId;
	char                deviceToken[DEVICE_TOKEN_SIZE];
	char                messageKey[MESSAGE_KEY_SIZE];
	char                messageArguments[MESSAGE_ARGUMENTS_SIZE];
};

int
ResetInMessangerFlag(void);

int
NumberOfOutstandingNotifications(void);

int
FetchListOfOutstandingNotifications(void);

int
FetchNotificationsToMessanger(void);

int
MoveOutstandingToInTheAir(void);

int
FlagSentNotification(void);

int
ReleaseProcessedNotificationsFromMessanger(void);
