#ifndef _TASKS_
#define _TASKS_

#include <poll.h>
#include <semaphore.h>
#include "buffers.h"

#define TaskStatusGood							0x00000000
#define TaskStatusOutOfMemory					0x00000001
#define TaskStatusPollForReceiveError			0x00000002
#define TaskStatusPollForReceiveFailed			0x00000004
#define TaskStatusPollForReceiveTimeout			0x00000008
#define TaskStatusNoDataReceived				0x00000010
#define TaskStatusReadFromSocketFailed			0x00000020
#define TaskStatusMissingPaquetPilot			0x00000040
#define TaskStatusMissingPaquetSignature		0x00000080
#define TaskStatusReceivedDataIncomplete		0x00000100
#define TaskStatusCannotCreatePaquetThread		0x00000200
#define TaskStatusPollForSendError				0x00000400
#define TaskStatusPollForSendFailed				0x00000800
#define TaskStatusPollForSendTimeout			0x00001000
#define TaskStatusNoDataSent					0x00002000
#define TaskStatusWriteToSocketFailed			0x00004000
#define TaskStatusWrongPayloadSize				0x00008000
#define TaskStatusNoDatabaseHandlers			0x00010000
#define TaskStatusUnexpectedDatabaseResult		0x00020000
#define TaskStatusMissingDialogueDemande		0x00040000
#define TaskStatusMissingAnticipantRecord		0x00080000
#define TaskStatusCannotSendDialogueVerdict		0x00100000
#define TaskStatusDeviceAuthenticationFailed	0x00200000
#define TaskStatusProfileAuthenticationFailed	0x00400000
#define TaskStatusCannotGetSession				0x00800000
#define TaskStatusCannotCreateSession			0x01000000
#define TaskStatusCannotAllocateBufferForInput	0x10000000
#define TaskStatusCannotAllocateBufferForOutput	0x20000000
#define TaskStatusCannotExtendBufferForInput	0x40000000

typedef struct task {
	int					taskId;
	uint64_t			deviceId;
	uint64_t			profileId;
	uint64_t			sessionId;
	int					status;
	pthread_spinlock_t	statusLock;
	pthread_spinlock_t	heavyJobLock;
	pthread_spinlock_t	downloadLock;
	pthread_t			thread;
	int					sockFD;
	char				*clientIP;
} task_t;

typedef struct paquet {
	struct task			*task;
	pthread_t			thread;
	struct pollfd		pollFD;
	int					paquetId;
	int					commandCode;
	uint32_t			payloadSize;
	struct buffer		*inputBuffer;
	struct buffer		*outputBuffer;
} paquet_t;

inline struct task *startTask(int sockFD, char *clientIP);

#endif
