#ifndef __APNS__
#define __APNS__

#include <openssl/ssl.h>

#include <postgres.h>

#include "notification.h"

#define RC_OK                   0
#define RC_ERROR                -1
#define RC_CANNOT_CONNECT       1
#define RC_ALREADY_CONNECTED    2
#define RC_RESOURCES_BUSY       3
#define RC_XMIT_ERROR           4

struct APNS_Connection
{
    X509        *cert;
    SSL_CTX     *ctx;
    SSL         *ssl;
    int         sockFD;
};

#pragma pack(push, 1)
struct apnsMessage
{
    uint8       commandCode;
    uint16      deviceTokenLength;
    uint8       deviceToken[DEVICE_TOKEN_SIZE];
    uint16      payloadLength;
    char        payload[];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct apnsFrame
{
    uint8       commandCode;
    uint32      frameLength;
    uint8       frameData[];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct apnsFrameItem
{
    uint8       itemId;
    uint16      itemDataLength;
    uint8       itemData[];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct apnsFrameNotification
{
    uint8       deviceToken[DEVICE_TOKEN_SIZE];
    uint8       payload[];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct apnsFrameNotificationFooter
{
    uint32      notificationId;
    uint32      expirationDate;
    uint8       priority;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct apnsResponse
{
    uint8       commandCode;
    uint8       status;
    uint32      notificationId;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct apnsFeedbackTuple
{
    uint32      timestamp;
    uint16      deviceTokenLength;
    uint8       deviceToken[DEVICE_TOKEN_SIZE];
};
#pragma pack(pop)

#define APNS_RESPONSE_COMMAND_CODE                  8

#define APNS_RESPONSE_STATUS_OK                     0
#define APNS_RESPONSE_STATUS_PROCESSING_ERROR       1
#define APNS_RESPONSE_STATUS_MISSING_DEVICE_TOKEN   2
#define APNS_RESPONSE_STATUS_MISSING_TOPIC          3
#define APNS_RESPONSE_STATUS_MISSING_PAYLOAD        4
#define APNS_RESPONSE_STATUS_INVALID_TOKEN_SIZE     5
#define APNS_RESPONSE_STATUS_INVALID_TOPIC_SIZE     6
#define APNS_RESPONSE_STATUS_INVALID_PAYLOAD_SIZE   7
#define APNS_RESPONSE_STATUS_INCALID_TOKEN          8
#define APNS_RESPONSE_STATUS_SHUTDOWN               10
#define APNS_RESPONSE_STATUS_UNKNOWN                255

#define APNS_DISCONNECT_IF_IDLE         60
#define SLEEP_ON_CONNECT_ERROR          5
#define SLEEP_ON_XMIT_ERROR             10
#define SLEEP_ON_BUSY_RESOURCES         2
#define SLEEP_ON_OTHER_ERROR            10
#define APNS_MAX_PAYLOAD_LENGTH         2048

#define MAX(a, b) ((a) > (b) ? (a) : (b))

void
KnockKnock(void);

int
ConnectToAPNS(struct APNS_Connection *connection);

void
DisconnectFromAPNS(struct APNS_Connection *connection);

int
SendOneByOne(struct APNS_Connection *connection);

int
SendAsFrame(struct APNS_Connection *connection);

#endif
