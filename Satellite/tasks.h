#ifndef __TASKS__
#define __TASKS__

#include <poll.h>
#include <pthread.h>

#include "api.h"
#include "desk.h"
#include "mmps.h"
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

#pragma pack(push, 1)
struct dialogueDemande
{
	uint64  		dialogueSignature;
	double			deviceTimestamp;
	uint32  		dialogueType;
	uint8  			applicationVersion;
	uint8  			applicationSubersion;
	uint16  		applicationRelease;
	uint16  		deviceType;
	char			applicationBuild[6];
	char			deviceToken[TokenBinarySize];
	char			profileToken[TokenBinarySize];
	char			sessionToken[TokenBinarySize];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct dialogueVerdict
{
	uint64  		dialogueSignature;
	uint32  		verdictCode;
	char			sessionToken[TokenBinarySize];
};
#pragma pack(pop)

struct revisions
{
	uint32				onRadar;
	uint32				inSight;
	uint32				onMap;
};

struct task
{
	struct MMPS_Buffer	*containerBuffer;
	struct desk     	*desk;
	pthread_t			thread;
	int					taskId;
	uint64  			deviceId;
	uint64  			profileId;
	uint64  			sessionId;
	pthread_spinlock_t	statusLock;
	long				status;
	char				clientIP[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

	struct
	{
		struct dialogueDemande	demande;
		struct dialogueVerdict	verdict;
	} dialogue;

	struct
	{
		int					sockFD;
		pthread_mutex_t		receiveMutex;
		pthread_mutex_t		sendMutex;
	} xmit;

	struct
	{
		pthread_spinlock_t	chainLock;
		pthread_spinlock_t	heavyJobLock;
		pthread_mutex_t		downloadMutex;
		struct paquet		*chainAnchor;
	} paquet;

	struct
	{
		struct revisions	lastKnownRevision;
		struct revisions	currentRevision;
		struct paquet		*broadcastPaquet;
		pthread_mutex_t		editMutex;
		pthread_mutex_t		waitMutex;
		pthread_cond_t		waitCondition;
	} broadcast;
};

struct paquet
{
	struct MMPS_Buffer	*containerBuffer;
	struct task			*task;
	struct paquet		*nextInChain;
	pthread_t			thread;
	struct pollfd		pollFD;
	int					paquetId;
	int					commandCode;
	uint32				payloadSize;
	struct MMPS_Buffer	*inputBuffer;
	struct MMPS_Buffer	*outputBuffer;
};

struct task *
startTask(
	struct desk		*desk,
	int				sockFD,
	char			*clientIP);

#define setTaskStatus(task, statusMask) \
do { reportDebug("Task (%s) set status 0x%016lX", __FUNCTION__, statusMask); __setTaskStatus(task, statusMask); } while (0)

inline void
__setTaskStatus(struct task *task, long statusMask);

inline long
getTaskStatus(struct task *task);

void
appentPaquetToTask(struct task *task, struct paquet *paquet);

void
removePaquetFromTask(struct task *task, struct paquet *paquet);

#endif
