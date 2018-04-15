#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <postgres.h>
#include <storage/proc.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "apns.h"
#include "chalkboard.h"
#include "mmps.h"
#include "notification.h"
#include "report.h"

#undef SANDBOX

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

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

static void
hexDump(char *message, int size);

static int
ResolveAddress(const char *hostname, in_addr_t *address);

static int
PrepareMessage(struct Notification *notification, struct apnsMessage *message);

void
KnockKnock(void)
{
    int rc;

    ReportInfo("Messenger knock... knock...");

    rc = pthread_mutex_lock(&chalkboard->apns.readyToGoMutex);
    if (rc != 0)
    {
        ReportError("Error has occurred on mutex lock: rc=%d", rc);
        return;
    }

    rc = pthread_cond_signal(&chalkboard->apns.readyToGoCond);
    if (rc != 0)
    {
        pthread_mutex_unlock(&chalkboard->apns.readyToGoMutex);
        ReportError("Error has occurred on condition signal: rc=%d", rc);
        return;
    }

    rc = pthread_mutex_unlock(&chalkboard->apns.readyToGoMutex);
    if (rc != 0)
    {
        ReportError("Error has occurred on mutex unlock: rc=%d", rc);
        return;
    }
}

static void
hexDump(char *message, int size)
{
	char decoded[1024];
	int i;

	bzero(decoded, sizeof(decoded));

    for (i = 0; i < size; i++)
	{
		char *destination = (char *) &decoded + i * 3;
    	unsigned char source = (unsigned char) message[i];
	    sprintf(destination, "%02X ", source);
	}

    ReportInfo("DUMP: %s", (char *)&decoded);
}

static int
ResolveAddress(const char *hostname, in_addr_t *address)
{
    struct hostent  *he;
    struct in_addr  **addressList;
    char            ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

    he = gethostbyname(hostname);
    if (he == NULL)
        return -1;

    addressList = (struct in_addr **) he->h_addr_list;
    if (addressList == NULL)
        return -1;

    strcpy(ip, inet_ntoa(*addressList[0]));
    if (strlen(ip) == 0)
        return -1;

    *address = inet_addr(ip);

    return RC_OK;
}

static int
PrepareMessage(struct Notification *notification, struct apnsMessage *message)
{
    int messageLength;
    int payloadLength;

    message->commandCode = 0;
    message->deviceTokenLength = be16toh(DEVICE_TOKEN_SIZE);
    memcpy(&message->deviceToken, notification->deviceToken, DEVICE_TOKEN_SIZE);

    sprintf((char *) &message->payload,
        "{\"aps\":{\"alert\":{\"loc-key\":\"%s\",\"loc-args\":[%s]},\"sound\":\"default\"}}",
        notification->messageKey,
        notification->messageArguments);

    ReportInfo("%s", message->payload);

    payloadLength = strlen(message->payload);

    message->payloadLength = be16toh(payloadLength);

    messageLength = sizeof(struct apnsMessage) + payloadLength;

    return messageLength;
}

int
ConnectToAPNS(struct APNS_Connection *connection)
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
    if (SSL_library_init() < 0)
    {
        ReportError("Could not initialize OpenSSL library.");
        return RC_CANNOT_CONNECT;
    }

    // Set SSLv2 client hello, also announce SSLv3 and TLSv1.
    //
    method = SSLv23_client_method();

    // Create a new SSL context.
    //
    connection->ctx = SSL_CTX_new(method);
    if (connection->ctx == NULL)
    {
        ReportError("Unable to create a new SSL context structure.");
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
	if (connection->ssl == NULL)
	{
		ReportError("Cannot create a new SSL structure");
		return RC_CANNOT_CONNECT;
	}

    // Make the underlying TCP socket connection.
    //
	connection->sockFD = socket(AF_INET, SOCK_STREAM, 0);
	if (connection->sockFD < 0)
	{
		ReportError("Cannot open a socket: errno=%d", errno);
		return RC_CANNOT_CONNECT;
	}

	bzero((char *)&apnsAddress, sizeof(apnsAddress));

	apnsAddress.sin_family = AF_INET;
	apnsAddress.sin_port = htons(APNS_GATEWAY_PORT);

	if (ResolveAddress(APNS_GATEWAY_HOST, &apnsAddress.sin_addr.s_addr) != RC_OK)
	{
		ReportError("Cannot resolve server address");
		return RC_CANNOT_CONNECT;
	}

	if (connect(connection->sockFD, (struct sockaddr *) &apnsAddress, sizeof(apnsAddress)) < 0)
	{
		ReportError("Cannot connect to socket: errno=%d", errno);
		return RC_CANNOT_CONNECT;
	}

    // Attach the SSL session to the socket descriptor.
    //
    SSL_set_fd(connection->ssl, connection->sockFD);

    // Try to SSL-connect here, returns 1 for success             *
    //
    if (SSL_connect(connection->ssl) != 1)
    {
        ReportError("Could not build SSL session.");
        return RC_CANNOT_CONNECT;
    }

    // Get the remote certificate into the X509 structure.
    //
    connection->cert = SSL_get_peer_certificate(connection->ssl);
    if (connection->cert == NULL)
    {
        ReportError("Could not get a certificate");
        return RC_CANNOT_CONNECT;
    }

    ReportInfo("Connected to APNS");

    return RC_OK;
}

void
DisconnectFromAPNS(struct APNS_Connection *connection)
{
    // Release resources.
    //
    if (connection->ssl != NULL)
    {
        SSL_free(connection->ssl);
        connection->ssl = NULL;
    }

    if (connection->sockFD > 0)
    {
        close(connection->sockFD);
        connection->sockFD = 0;
    }

    if (connection->cert != NULL)
    {
        X509_free(connection->cert);
        connection->cert = NULL;
    }

    if (connection->ctx != NULL)
    {
        SSL_CTX_free(connection->ctx);
        connection->ctx = NULL;
    }

    ReportInfo("Disconnected from APNS");
}

int
SendOneByOne(struct APNS_Connection *connection)
{
	struct MMPS_Buffer      *notificationBuffer;
    struct Notification     *notification;
	struct MMPS_Buffer      *messageBuffer;
    struct apnsMessage      *message;
    int                     messageSize;
    int                     bytesSent;

    messageBuffer = MMPS_PeekBuffer(chalkboard->pools.apns, BUFFER_XMIT);
    if (messageBuffer == NULL)
    {
        ReportInfo("APNS thread cannot get a buffer");
        return RC_RESOURCES_BUSY;
    }

    message = (struct apnsMessage *) messageBuffer->data;

    if (pthread_mutex_trylock(&chalkboard->inTheAirNotifications.mutex) != 0)
    {
        ReportInfo("APNS thread cannot begin transmit because queue is locked");
        return RC_RESOURCES_BUSY;
    }

    ReportInfo("APNS thread has begun transmit");

    while ((notificationBuffer = chalkboard->inTheAirNotifications.buffers) != NULL)
    {
        notification = (struct Notification *) notificationBuffer->data;

        messageSize = PrepareMessage(notification, message);
        bytesSent = SSL_write(connection->ssl, message, messageSize);
        if (bytesSent != messageSize)
        {
            ReportInfo("Cannot send message: sent %d of %d bytes.",
                bytesSent, messageSize);

            pthread_mutex_unlock(&chalkboard->inTheAirNotifications.mutex);

            MMPS_PokeBuffer(messageBuffer);

            return RC_XMIT_ERROR;
        }

        ReportInfo("APNS thread has sent %d bytes", bytesSent);

        chalkboard->inTheAirNotifications.buffers = MMPS_NextBuffer(notificationBuffer);

        notificationBuffer->next = NULL;

        pthread_mutex_lock(&chalkboard->sentNotifications.mutex);
        chalkboard->sentNotifications.buffers =
            MMPS_AppendBuffer(chalkboard->sentNotifications.buffers, notificationBuffer);
        pthread_mutex_unlock(&chalkboard->sentNotifications.mutex);
    }

    pthread_mutex_unlock(&chalkboard->inTheAirNotifications.mutex);

    MMPS_PokeBuffer(messageBuffer);

    ReportInfo("APNS thread has complete transmit");

    return RC_OK;
}

int
SendAsFrame(struct APNS_Connection *connection)
{
    struct MMPS_Buffer                  *notificationBuffer;
    struct Notification                 *notification;
    struct MMPS_Buffer                  *messageBuffer;
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

    messageBuffer = MMPS_PeekBuffer(chalkboard->pools.apns, BUFFER_XMIT);
    if (messageBuffer == NULL)
    {
        ReportInfo("APNS thread cannot get a buffer");
        return RC_RESOURCES_BUSY;
    }

    if (pthread_mutex_trylock(&chalkboard->inTheAirNotifications.mutex) != 0)
    {
        ReportInfo("APNS thread cannot begin transmit because queue is locked");
        return RC_RESOURCES_BUSY;
    }

    ReportInfo("APNS thread has begun transmit");

    frame = (struct apnsFrame *) messageBuffer->data;

    frame->commandCode = 2;

    frameLength = 0;

    while ((notificationBuffer = chalkboard->inTheAirNotifications.buffers) != NULL)
    {
        notification = (struct Notification *) notificationBuffer->data;

        frameItem = (void *) ((unsigned long) &frame->frameData + (unsigned long) frameLength);

        frameItem->itemId = 2;

        frameNotification = (struct apnsFrameNotification *) &frameItem->itemData;

        memcpy(&frameNotification->deviceToken, notification->deviceToken, DEVICE_TOKEN_SIZE);

        sprintf((char *) &frameNotification->payload,
            "{\"aps\":{\"alert\":{\"loc-key\":\"%s\",\"loc-args\":[%s]},\"sound\":\"default\"}}",
            notification->messageKey,
            notification->messageArguments);

        itemDataLength = sizeof(struct apnsFrameNotification);
        itemDataLength += strlen((char *) &frameNotification->payload);

        frameNotificationFooter = (void *) ((unsigned long) frameNotification + (unsigned long) itemDataLength);
        frameNotificationFooter->notificationId = be32toh(2);
        frameNotificationFooter->expirationDate = 0;
        frameNotificationFooter->priority = 10;

        itemDataLength += sizeof(struct apnsFrameNotificationFooter);

        frameItem->itemDataLength = be16toh(itemDataLength);

        frameLength += sizeof(struct apnsFrameItem) + itemDataLength;

        chalkboard->inTheAirNotifications.buffers = MMPS_NextBuffer(notificationBuffer);

        notificationBuffer->next = NULL;

        pthread_mutex_lock(&chalkboard->sentNotifications.mutex);
        chalkboard->sentNotifications.buffers =
            MMPS_AppendBuffer(chalkboard->sentNotifications.buffers, notificationBuffer);
        pthread_mutex_unlock(&chalkboard->sentNotifications.mutex);

/* FIXME
        if (frameLength >= (POOL_APNS_SIZE_OF_BUFFER * 0.8))
            break;
*/
    }

    frame->frameLength = be32toh(frameLength);

    pthread_mutex_unlock(&chalkboard->inTheAirNotifications.mutex);

    bytesToSend = sizeof(struct apnsFrame) + frameLength;

    bytesSent = SSL_write(connection->ssl, frame, bytesToSend);
    if (bytesSent != bytesToSend)
    {
        ReportError("Error has occurred by send: sent %d of %d bytes, errno=%d",
            bytesSent, bytesToSend, errno);
        return RC_XMIT_ERROR;
    }
#ifdef APNS_XMIT
    ReportInfo("Sent %d bytes", bytesSent);
#endif

    // Look for response
    //
    response = (struct apnsResponse *) messageBuffer->data;
    do {
        bytesRead = SSL_read(connection->ssl, response, sizeof(struct apnsResponse));
#ifdef APNS_XMIT
        ReportInfo("Read %d bytes", bytesRead);
#endif
        if (bytesRead == 0)
        {
            break;
        }
        else if ((bytesRead % sizeof(struct apnsResponse)) == 0)
        {
            ReportInfo("Response %d %d 0x%08X",
                response->commandCode, response->status, response->notificationId);
        }
        else
        {
            ReportError("Error has occurred by read from socket: read %d bytes, errno=%d",
                bytesRead, errno);
            return RC_XMIT_ERROR;
        }
    } while (bytesRead > 0);

    MMPS_PokeBuffer(messageBuffer);

    ReportInfo("APNS thread has complete transmit");

    return RC_OK;
}
