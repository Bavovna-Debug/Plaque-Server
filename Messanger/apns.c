#include <errno.h>
#include <netdb.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <postgres.h>
#include <storage/proc.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "apns.h"
#include "buffers.h"
#include "desk.h"
#include "notification.h"
#include "report.h"

#define SANDBOX

#ifdef SANDBOX
#define APNS_GATEWAY_HOST           "gateway.sandbox.push.apple.com"
#define APNS_GATEWAY_PORT           2195
#define APNS_FEEDBACK_HOST          "feedback.sandbox.push.apple.com"
#define APNS_FEEDBACK_PORT          2196
#define APNS_CERT                   "/opt/vp/apns/apns-dev.pem"
#else
#define APNS_GATEWAY_HOST           "gateway.push.apple.com"
#define APNS_GATEWAY_PORT           2195
#define APNS_FEEDBACK_HOST          "feedback.push.apple.com"
#define APNS_FEEDBACK_PORT          2196
#define APNS_CERT                   "/opt/vp/apns/apns-dev.pem"
#endif

typedef struct connection {
    X509        *cert;
    SSL_CTX     *ctx;
    SSL         *ssl;
    int         sockFD;
} deskAPNS_t;

#pragma pack(push, 1)
typedef struct apnsMessage {
    uint8       commandCode;
    uint16      deviceTokenLength;
    uint8       deviceToken[DEVICE_TOKEN_SIZE];
    uint16      payloadLength;
    char        payload[];
} apnsMessage_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct apnsFrame {
    uint8       commandCode;
    uint32      frameLength;
    uint8       frameData[];
} apnsFrame_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct apnsFrameItem {
    uint8       itemId;
    uint16      itemDataLength;
    uint8       itemData[];
} apnsFrameItem_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct apnsFrameNotification {
    uint8       deviceToken[DEVICE_TOKEN_SIZE];
    uint8       payload[];
} apnsFrameNotification_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct apnsFrameNotificationFooter {
    uint32      notificationId;
    uint32      expirationDate;
    uint8       priority;
} apnsFrameNotificationFooter_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct apnsResponse {
    uint8       commandCode;
    uint8       status;
    uint32      notificationId;
} apnsResponse_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct apnsFeedbackTuple {
    uint32      timestamp;
    uint16      deviceTokenLength;
    uint8       deviceToken[DEVICE_TOKEN_SIZE];
} apnsFeedbackTuple_t;
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

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define RC_OK                   0
#define RC_ERROR                -1
#define RC_CANNOT_CONNECT       1
#define RC_ALREADY_CONNECTED    2
#define RC_RESOURCES_BUSY       3
#define RC_XMIT_ERROR           4

static void
hexDump(char *message, int size);

static int
resolveAddress(const char *hostname, in_addr_t *address);

static int
prepareMessage(struct notification *notification, struct apnsMessage *message);

static int
connectToAPNS(struct connection *connection);

static void
disconnectFromAPNS(struct connection *connection);

static int
apnsSendOneByOne(struct desk *desk, struct connection *connection);

static int
apnsSendAsFrame(struct desk *desk, struct connection *connection);

void *
apnsThread(void *arg)
{
	struct desk *desk = (struct desk *)arg;
	struct connection   connection;
	struct timespec     ts;
	int                 rc;

    bzero(&connection, sizeof(connection));

    rc = RC_OK;

    while (1)
    {
        // Wait for semaphore only if this is the first run or if the previous run was successful.
        //
        if (rc == RC_OK) {
            // Prepare timer for semaphore.
            //
            rc = clock_gettime(CLOCK_REALTIME_COARSE, &ts);
            if (rc == -1) {
                reportError("Cannot get system time: errno=%d", errno);
                rc = RC_ERROR;
                break;
            }

            ts.tv_sec += APNS_DISCONNECT_IF_IDLE;

            // Wait for semaphore for some time.
            //
            reportLog("APNS thread whating %d seconds for pending notifications",
                APNS_DISCONNECT_IF_IDLE);
            rc = sem_timedwait(desk->apns.readyToGo, &ts);
            if (rc == -1) {
                if (errno != ETIMEDOUT) {
                    //
                    // Semaphore error! Break the loop.
                    //
                    reportError("Error has ocurred while whaiting for timed semaphore: errno=%d", errno);
                    rc = RC_ERROR;
                    break;
                } else {
                    //
                    // Semaphore has timed out.
                    // Disconnect from APNS and start waiting for semaphore without timer.
                    //
                    reportLog("No pending notifications");

                    disconnectFromAPNS(&connection);

                    reportLog("APNS thread waiting for pending notifications");
                    rc = sem_wait(desk->apns.readyToGo);
                    if (rc == -1) {
                        //
                        // Semaphore error! Break the loop.
                        //
                        reportError("Error has ocurred while whaiting for semaphore: errno=%d", errno);
                        rc = RC_ERROR;
                        break;
                    }
                }
            }
        }

        // Send pending notifications.
        // Retry if some resources are busy.
        //
        do {
            // Establish connection to APNS.
            //
            rc = connectToAPNS(&connection);
            if (rc != RC_OK) {
                if (rc != RC_ALREADY_CONNECTED) {
                    //
                    // If any kind of connection error has occurred,
                    // then wait a bit before retrying to connect.
                    //
                    sleep(SLEEP_ON_CONNECT_ERROR);
                    continue;
                }
            }

            rc = apnsSendOneByOne(desk, &connection);
            //rc = apnsSendAsFrame(desk, &connection);
            if (rc != RC_OK) {
                if (rc == RC_RESOURCES_BUSY) {
                    //
                    // If some resources are busy,
                    // then wait a bit before retrying to connect.
                    //
                    sleep(SLEEP_ON_BUSY_RESOURCES);
                } else if (rc == RC_XMIT_ERROR) {
                    //
                    // In case of send/receive error disconnect
                    // and wait a bit before retrying to connect.
                    //
                    disconnectFromAPNS(&connection);
                    sleep(SLEEP_ON_XMIT_ERROR);
                } else {
                    break;
                }
            }
        } while (rc != RC_OK);

        // In case of any I/O error disconnect and wait a bit.
        //
        if (rc != RC_OK) {
            disconnectFromAPNS(&connection);
            sleep(SLEEP_ON_OTHER_ERROR);
        }
    }

	pthread_exit(NULL);
}

void
apnsKnockKnock(struct desk *desk)
{
    reportLog("Messenger knock... knock...");
    sem_post(desk->apns.readyToGo);
}

static void
hexDump(char *message, int size)
{
	char decoded[1024];
	int i;

	bzero(decoded, sizeof(decoded));

    	for (i = 0; i < size; i++)
	    {
		char *destination = (char *)&decoded + i * 3;
    		unsigned char source = (unsigned char)message[i];
	    	sprintf(destination, "%02X ", source);
	    }

    reportLog("DUMP: %s", (char *)&decoded);
}

static int
resolveAddress(const char *hostname, in_addr_t *address)
{
    struct hostent  *he;
    struct in_addr  **addressList;
    char            ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

    he = gethostbyname(hostname);
    if (he == NULL)
        return -1;

    addressList = (struct in_addr **)he->h_addr_list;
    if (addressList == NULL)
        return -1;

    strcpy(ip, inet_ntoa(*addressList[0]));
    if (strlen(ip) == 0)
        return -1;

    *address = inet_addr(ip);

    return RC_OK;
}

static int
prepareMessage(struct notification *notification, struct apnsMessage *message)
{
    int messageLength;
    int payloadLength;

    message->commandCode = 0;
    message->deviceTokenLength = be16toh(DEVICE_TOKEN_SIZE);
    memcpy(&message->deviceToken, notification->deviceToken, DEVICE_TOKEN_SIZE);

    sprintf((char *)&message->payload, "{\"aps\":{\"alert\":{\"loc-key\":\"%s\",\"loc-args\":[%s]},\"sound\":\"default\"}}",
        notification->messageKey,
        notification->messageArguments);
reportLog("%s", message->payload);

    payloadLength = strlen(message->payload);

    message->payloadLength = be16toh(payloadLength);

    messageLength = sizeof(struct apnsMessage) + payloadLength;

    return messageLength;
}

static int
connectToAPNS(struct connection *connection)
{
    const SSL_METHOD    *method;
	struct sockaddr_in  apnsAddress;

    // Do nothing if already connected.
    //
    if (connection->sockFD > 0)
        return 0;

    // Initialize OpenSSL.
    //
    OpenSSL_add_all_algorithms();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
    SSL_load_error_strings();

    // Initialize SSL library and register algorithms.
    //
    if (SSL_library_init() < 0) {
        reportError("Could not initialize OpenSSL library.");
        return RC_CANNOT_CONNECT;
    }

    // Set SSLv2 client hello, also announce SSLv3 and TLSv1.
    //
    method = SSLv23_client_method();

    // Create a new SSL context.
    //
    connection->ctx = SSL_CTX_new(method);
    if (connection->ctx == NULL) {
        reportError("Unable to create a new SSL context structure.");
        return RC_CANNOT_CONNECT;
    }

    // Disabling SSLv2 will leave v3 and TSLv1 for negotiation.
    //
    SSL_CTX_set_options(connection->ctx, SSL_OP_NO_SSLv2);
    SSL_CTX_use_certificate_file(connection->ctx, APNS_CERT, SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(connection->ctx, APNS_CERT, SSL_FILETYPE_PEM);

    // Create new SSL connection state object.
    //
    connection->ssl = SSL_new(connection->ctx);
	if (connection->ssl == NULL) {
		reportError("Cannot create a new SSL structure");
		return RC_CANNOT_CONNECT;
	}

    // Make the underlying TCP socket connection.
    //
	connection->sockFD = socket(AF_INET, SOCK_STREAM, 0);
	if (connection->sockFD < 0) {
		reportError("Cannot open a socket: errno=%d", errno);
		return RC_CANNOT_CONNECT;
	}

	bzero((char *)&apnsAddress, sizeof(apnsAddress));

	apnsAddress.sin_family = AF_INET;
	apnsAddress.sin_port = htons(APNS_GATEWAY_PORT);

	if (resolveAddress(APNS_GATEWAY_HOST, &apnsAddress.sin_addr.s_addr) != RC_OK) {
		reportError("Cannot resolve server address");
		return RC_CANNOT_CONNECT;
	}

	if (connect(connection->sockFD, (struct sockaddr *)&apnsAddress, sizeof(apnsAddress)) < 0) {
		reportError("Cannot connect to socket: errno=%d", errno);
		return RC_CANNOT_CONNECT;
	}

    // Attach the SSL session to the socket descriptor.
    //
    SSL_set_fd(connection->ssl, connection->sockFD);

    // Try to SSL-connect here, returns 1 for success             *
    //
    if (SSL_connect(connection->ssl) != 1) {
        reportError("Could not build SSL session.");
        return RC_CANNOT_CONNECT;
    }

    // Get the remote certificate into the X509 structure.
    //
    connection->cert = SSL_get_peer_certificate(connection->ssl);
    if (connection->cert == NULL) {
        reportError("Could not get a certificate");
        return RC_CANNOT_CONNECT;
    }

    reportLog("Connected to APNS");

    return RC_OK;
}

static void
disconnectFromAPNS(struct connection *connection)
{
    // Release resources.
    //
    if (connection->ssl != NULL) {
        SSL_free(connection->ssl);
        connection->ssl = NULL;
    }

    if (connection->sockFD > 0) {
        close(connection->sockFD);
        connection->sockFD = 0;
    }

    if (connection->cert != NULL) {
        X509_free(connection->cert);
        connection->cert = NULL;
    }

    if (connection->ctx != NULL) {
        SSL_CTX_free(connection->ctx);
        connection->ctx = NULL;
    }

    reportLog("Disconnected from APNS");
}


static int
apnsSendOneByOne(struct desk *desk, struct connection *connection)
{
	struct buffer       *notificationBuffer;
    struct notification *notification;
	struct buffer       *messageBuffer;
    struct apnsMessage  *message;
    int                 messageSize;
    int                 bytesSent;

    messageBuffer = peekBuffer(desk->pools.apns);
    if (messageBuffer == NULL) {
        reportLog("APNS thread cannot get a buffer");
        return RC_RESOURCES_BUSY;
    }

    message = (struct apnsMessage *)messageBuffer->data;

    if (pthread_spin_trylock(&desk->inTheAirNotifications.lock) != 0) {
        reportLog("APNS thread cannot begin transmit because queue is locked");
        return RC_RESOURCES_BUSY;
    }

    reportLog("APNS thread has begun transmit");

    while ((notificationBuffer = desk->inTheAirNotifications.buffers) != NULL)
    {
        notification = (struct notification *)notificationBuffer->data;

        messageSize = prepareMessage(notification, message);
        bytesSent = SSL_write(connection->ssl, message, messageSize);
        if (bytesSent != messageSize) {
            reportLog("Cannot send message: sent %d of %d bytes.",
                bytesSent, messageSize);

            pthread_spin_unlock(&desk->inTheAirNotifications.lock);

            return RC_XMIT_ERROR;
        }

        reportLog("APNS thread has sent %d bytes", bytesSent);

        desk->inTheAirNotifications.buffers = nextBuffer(notificationBuffer);

        notificationBuffer->next = NULL;

        pthread_spin_lock(&desk->sentNotifications.lock);
        desk->sentNotifications.buffers = appendBuffer(desk->sentNotifications.buffers, notificationBuffer);
        pthread_spin_unlock(&desk->sentNotifications.lock);
    }

    pthread_spin_unlock(&desk->inTheAirNotifications.lock);

    pokeBuffer(messageBuffer);

    reportLog("APNS thread has complete transmit");

    return RC_OK;
}

static int
apnsSendAsFrame(struct desk *desk, struct connection *connection)
{
    struct buffer                       *notificationBuffer;
    struct notification                 *notification;
    struct buffer                       *messageBuffer;
    struct apnsFrame                    *frame;
    struct apnsFrameItem                *frameItem;
    struct apnsFrameNotification        *frameNotification;
    struct apnsFrameNotificationFooter  *frameNotificationFooter;
    struct apnsResponse                 *response;
    uint32                              frameLength;
    uint16                              itemDataLength;
    int                                 bytesToSend;
    int                                 bytesSent;
    int                                 bytesRead;

    messageBuffer = peekBuffer(desk->pools.apns);
    if (messageBuffer == NULL) {
        reportLog("APNS thread cannot get a buffer");
        return RC_RESOURCES_BUSY;
    }

    if (pthread_spin_trylock(&desk->inTheAirNotifications.lock) != 0) {
        reportLog("APNS thread cannot begin transmit because queue is locked");
        return RC_RESOURCES_BUSY;
    }

    reportLog("APNS thread has begun transmit");

    frame = (struct apnsFrame *)messageBuffer->data;

    frame->commandCode = 2;

    frameLength = 0;

    while ((notificationBuffer = desk->inTheAirNotifications.buffers) != NULL)
    {
        notification = (struct notification *)notificationBuffer->data;

        frameItem = (void *)((unsigned long)&frame->frameData + (unsigned long)frameLength);

        frameItem->itemId = 2;

        frameNotification = (struct apnsFrameNotification *)&frameItem->itemData;

        memcpy(&frameNotification->deviceToken, notification->deviceToken, DEVICE_TOKEN_SIZE);

        sprintf((char *)&frameNotification->payload,
            "{\"aps\":{\"alert\":{\"loc-key\":\"%s\",\"loc-args\":[%s]},\"sound\":\"default\"}}",
            notification->messageKey,
            notification->messageArguments);

        itemDataLength = sizeof(struct apnsFrameNotification);
        itemDataLength += strlen((char *)&frameNotification->payload);

        frameNotificationFooter = (void *)((unsigned long)frameNotification + (unsigned long)itemDataLength);
        frameNotificationFooter->notificationId = be32toh(2);
        frameNotificationFooter->expirationDate = 0;
        frameNotificationFooter->priority = 10;

        itemDataLength += sizeof(struct apnsFrameNotificationFooter);

        frameItem->itemDataLength = be16toh(itemDataLength);

        frameLength += sizeof(struct apnsFrameItem) + itemDataLength;

        desk->inTheAirNotifications.buffers = nextBuffer(notificationBuffer);

        notificationBuffer->next = NULL;

        pthread_spin_lock(&desk->sentNotifications.lock);
        desk->sentNotifications.buffers = appendBuffer(desk->sentNotifications.buffers, notificationBuffer);
        pthread_spin_unlock(&desk->sentNotifications.lock);

/* FIXME
        if (frameLength >= (POOL_APNS_SIZE_OF_BUFFER * 0.8))
            break;
*/
    }

    frame->frameLength = be32toh(frameLength);

    pthread_spin_unlock(&desk->inTheAirNotifications.lock);

    bytesToSend = sizeof(struct apnsFrame) + frameLength;

    bytesSent = SSL_write(connection->ssl, frame, bytesToSend);
    if (bytesSent != bytesToSend) {
        reportError("Error has occurred by send: sent %d of %d bytes, errno=%d",
            bytesSent, bytesToSend, errno);
        return RC_XMIT_ERROR;
    }
#ifdef APNS_XMIT
    reportLog("Sent %d bytes", bytesSent);
#endif

    // Look for response
    //
    response = (struct apnsResponse *)messageBuffer->data;
    do {
        bytesRead = SSL_read(connection->ssl, response, sizeof(struct apnsResponse));
#ifdef APNS_XMIT
        reportLog("Read %d bytes", bytesRead);
#endif
        if (bytesRead == 0) {
            break;
        } else if ((bytesRead % sizeof(struct apnsResponse)) == 0) {
            reportLog("Response %d %d 0x%08X",
                response->commandCode, response->status, response->notificationId);
        } else {
            reportError("Error has occurred by read from socket: read %d bytes, errno=%d",
                bytesRead, errno);
            return RC_XMIT_ERROR;
        }
    } while (bytesRead > 0);

    pokeBuffer(messageBuffer);

    reportLog("APNS thread has complete transmit");

    return RC_OK;
}
