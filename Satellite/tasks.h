#ifndef __TASKS__
#define __TASKS__

#include <poll.h>
#include <pthread.h>

#include "api.h"
#include "mmps.h"
#include "report.h"

#define TaskStatusGood							0x0000000000000000l
#define TaskStatusOutOfMemory					0x0000000000000001l
#define TaskStatusCannotCreatePaquetThread		0x0000000000000002l
#define TaskStatusNoDatabaseHandlers			0x0000000000000100l
#define TaskStatusUnexpectedDatabaseResult		0x0000000000000200l

#define TaskStatusCannotAllocateBufferForInput	0x0000000000000010l
#define TaskStatusCannotAllocateBufferForOutput	0x0000000000000020l
#define TaskStatusCannotExtendBufferForInput	0x0000000000000040l

#define TaskStatusDeviceAuthenticationFailed	0x0000000000010000l
#define TaskStatusProfileAuthenticationFailed	0x0000000000020000l
#define TaskStatusCannotGetSession				0x0000000000100000l
#define TaskStatusCannotCreateSession			0x0000000000200000l
#define TaskStatusCannotSetSessionOnline		0x0000000000400000l
#define TaskStatusCannotSetSessionOffline		0x0000000000800000l

#define TaskStatusPollForReceiveError			0x0000000100000000l
#define TaskStatusPollForReceiveFailed			0x0000000200000000l
#define TaskStatusPollForReceiveTimeout			0x0000000400000000l
#define TaskStatusNoDataReceived				0x0000000800000000l
#define TaskStatusReadFromSocketFailed			0x0000001000000000l
#define TaskStatusMissingPaquetPilot			0x0000002000000000l
#define TaskStatusMissingPaquetSignature		0x0000004000000000l
#define TaskStatusReceivedDataIncomplete		0x0000008000000000l

#define TaskStatusPollForSendError				0x0000010000000000l
#define TaskStatusPollForSendFailed				0x0000020000000000l
#define TaskStatusPollForSendTimeout			0x0000040000000000l
#define TaskStatusNoDataSent					0x0000080000000000l
#define TaskStatusWriteToSocketFailed			0x0000100000000000l
#define TaskStatusWrongPayloadSize				0x0000200000000000l
#define TaskStatusNoOutputDataProvided			0x0000400000000000l

#define TaskStatusMissingDialogueDemande		0x0100000000000000l
#define TaskStatusMissingAnticipantRecord		0x0200000000000000l
#define TaskStatusCannotSendDialogueVerdict		0x0400000000000000l

#define TaskStatusOtherError					0x8000000000000000l

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#pragma pack(push, 1)
struct DialogueDemande
{
	uint64  		dialogueSignature;
	double			deviceTimestamp;
	uint32  		dialogueType;
	uint8  			applicationVersion;
	uint8  			applicationSubersion;
	uint16  		applicationRelease;
	uint16  		deviceType;
	char			applicationBuild[6];
	char			deviceToken[API_TokenBinarySize];
	char			profileToken[API_TokenBinarySize];
	char			sessionToken[API_TokenBinarySize];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DialogueVerdict
{
	uint64  		dialogueSignature;
	uint32  		verdictCode;
	char			sessionToken[API_TokenBinarySize];
};
#pragma pack(pop)

struct Revisions
{
	uint32				onRadar;
	uint32				inSight;
	uint32				onMap;
};

struct Task
{
	struct MMPS_Buffer	*containerBuffer;
	pthread_t			thread;
	int					taskId;
	uint64  			deviceId;
	uint64  			profileId;
	uint64  			sessionId;
	pthread_spinlock_t	statusLock;
	uint64				status;
	char				clientIP[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

	struct
	{
		struct DialogueDemande	demande;
		struct DialogueVerdict	verdict;
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
		struct Paquet		*chainAnchor;
	} paquet;

	struct
	{
		struct Revisions	lastKnownRevision;
		struct Revisions	currentRevision;
		struct Paquet		*broadcastPaquet;
		pthread_mutex_t		editMutex;
		pthread_mutex_t		waitMutex;
		pthread_cond_t		waitCondition;
	} broadcast;
};

struct Paquet
{
	struct MMPS_Buffer	*containerBuffer;
	struct Task			*task;
	struct Paquet		*nextInChain;
	pthread_t			thread;
	struct pollfd		pollFD;
	int					paquetId;
	int					commandCode;
	uint32				payloadSize;
	struct MMPS_Buffer	*inputBuffer;
	struct MMPS_Buffer	*outputBuffer;
};

struct Task *
StartTask(
	int				sockFD,
	char			*clientIP);

inline void
__SetTaskStatus(struct Task *task, uint64 statusMask);

#define SetTaskStatus(task, statusMask) \
do { \
	ReportDebug("Task (%s) set status 0x%016luX", \
		__FUNCTION__, \
		statusMask); \
	__SetTaskStatus(task, statusMask); \
} while (0)

inline uint64
GetTaskStatus(struct Task *task);

void
AppentPaquetToTask(struct Task *task, struct Paquet *paquet);

void
RemovePaquetFromTask(struct Task *task, struct Paquet *paquet);

#endif
