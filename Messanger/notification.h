#ifndef _NOTIFICATION_
#define _NOTIFICATION_

#include "desk.h"

#define MAX_NOTIFICATIONS           5
#define DEVICE_TOKEN_SIZE           32
#define MESSAGE_KEY_SIZE            64
#define MESSAGE_ARGUMENTS_SIZE      1024

typedef struct notification {
    uint64              notificationId;
	uint64              deviceId;
	char                deviceToken[DEVICE_TOKEN_SIZE];
	char                messageKey[MESSAGE_KEY_SIZE];
	char                messageArguments[MESSAGE_ARGUMENTS_SIZE];
} notification_t;

int
resetInMessangerFlag(void);

int
numberOfOutstandingNotifications(void);

int
fetchListOfOutstandingNotifications(struct desk *desk);

int
fetchNotificationsToMessanger(struct desk *desk);

int
moveOutstandingToInTheAir(struct desk *desk);

int
flagSentNotification(struct desk *desk);

int
releaseProcessedNotificationsFromMessanger(struct desk *desk);

#endif
