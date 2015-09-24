#ifndef _TASKS_
#define _TASKS_

#include <poll.h>
#include <pthread.h>
#include <semaphore.h>

#include "buffers.h"
#include "desk.h"
#include "report.h"

#define TaskStatusGood							0x0000000000000000
#define TaskStatusOutOfMemory					0x0000000000000001
#define TaskStatusCannotCreatePaquetThread		0x0000000000000002
#define TaskStatusNoDatabaseHandlers			0x0000000000000100
#define TaskStatusUnexpectedDatabaseResult		0x0000000000000200

#define TaskStatusCannotAllocateBufferForInput	0x0000000000000010
#define TaskStatusCannotAllocateBufferForOutput	0x0000000000000020
#define TaskStatusCannotExtendBufferForInput	0x0000000000000040

#define TaskStatusDeviceAuthenticationFailed	0x0000000000010000
#define TaskStatusProfileAuthenticationFailed	0x0000000000020000
#define TaskStatusCannotGetSession				0x0000000000100000
#define TaskStatusCannotCreateSession			0x0000000000200000
#define TaskStatusCannotSetSessionOnline		0x0000000000400000
#define TaskStatusCannotSetSessionOffline		0x0000000000800000

#define TaskStatusPollForReceiveError			0x0000000100000000
#define TaskStatusPollForReceiveFailed			0x0000000200000000
#define TaskStatusPollForReceiveTimeout			0x0000000400000000
#define TaskStatusNoDataReceived				0x0000000800000000
#define TaskStatusReadFromSocketFailed			0x0000001000000000
#define TaskStatusMissingPaquetPilot			0x0000002000000000
#define TaskStatusMissingPaquetSignature		0x0000004000000000
#define TaskStatusReceivedDataIncomplete		0x0000008000000000

#define TaskStatusPollForSendError				0x0000010000000000
#define TaskStatusPollForSendFailed				0x0000020000000000
#define TaskStatusPollForSendTimeout			0x0000040000000000
#define TaskStatusNoDataSent					0x0000080000000000
#define TaskStatusWriteToSocketFailed			0x0000100000000000
#define TaskStatusWrongPayloadSize				0x0000200000000000
#define TaskStatusNoOutputDataProvided			0x0000400000000000

#define TaskStatusMissingDialogueDemande		0x0100000000000000
#define TaskStatusMissingAnticipantRecord		0x0200000000000000
#define TaskStatusCannotSendDialogueVerdict		0x0400000000000000

#define TaskStatusOtherError					0x8000000000000000

#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct revisions {
	uint32				onRadar;
	uint32				inSight;
	uint32				onMap;
} revisions_t;

typedef struct task {
	struct buffer		*containerBuffer;
	struct desk     	*desk;
	pthread_t			thread;
	int					taskId;
	uint64  			deviceId;
	uint64  			profileId;
	uint64  			sessionId;
	pthread_spinlock_t	statusLock;
	long				status;
	char				clientIP[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

	struct {
		int					sockFD;
		pthread_spinlock_t	receiveLock;
		pthread_spinlock_t	sendLock;
	} xmit;

	struct {
		pthread_spinlock_t	chainLock;
		pthread_spinlock_t	heavyJobLock;
		pthread_spinlock_t	downloadLock;
		struct paquet		*chainAnchor;
	} paquet;

	struct {
		struct revisions	lastKnownRevision;
		struct revisions	currentRevision;
		pthread_spinlock_t	lock;
		struct paquet		*broadcastPaquet;
		sem_t				waitForBroadcast;
	} broadcast;
} task_t;

typedef struct paquet {
	struct buffer		*containerBuffer;
	struct task			*task;
	struct paquet		*nextInChain;
	pthread_t			thread;
	struct pollfd		pollFD;
	int					paquetId;
	int					commandCode;
	uint32				payloadSize;
	struct buffer		*inputBuffer;
	struct buffer		*outputBuffer;
} paquet_t;

struct task *
startTask(
	struct desk		*desk,
	int				sockFD,
	char			*clientIP);

#define setTaskStatus(task, statusMask) \
do { reportLog("Task (%s) set status 0x%016lX", __FUNCTION__, statusMask); __setTaskStatus(task, statusMask); } while (0)

inline void
__setTaskStatus(struct task *task, long statusMask);

inline long
getTaskStatus(struct task *task);

void
appentPaquetToTask(struct task *task, struct paquet *paquet);

void
removePaquetFromTask(struct task *task, struct paquet *paquet);

#endif
