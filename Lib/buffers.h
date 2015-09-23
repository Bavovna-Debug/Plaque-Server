#ifndef _BUFFERS_
#define _BUFFERS_

#include <c.h>

#define	KB 1024
#define MB 1024 * 1024
#define GB 1024 * 1024 * 1024

#define MAX_BLOCK_SIZE	MB

#define BUFFERS_OUT_OF_MEMORY_CHAIN -1
#define BUFFERS_OUT_OF_MEMORY_BLOCK -1
#define BUFFERS_OUT_OF_MEMORY_DATA	-1

#ifdef BUFFERS_EYECATCHER
#define EYECATCHER_SIZE	16
#endif

typedef struct pool {
#ifdef BUFFERS_EYECATCHER
	char				eyeCatcher[EYECATCHER_SIZE];
#endif
	int					numberOfChains;
	struct chain		*chains[];
} pool_t;

typedef struct chain {
#ifdef BUFFERS_EYECATCHER
	char				eyeCatcher[EYECATCHER_SIZE];
#endif
	pthread_spinlock_t	lock;
	int					chainId;
	struct pool			*pool;
	int					numberOfBuffers;
	int					bufferSize;
	int					pilotSize;
	int					peekCursor;
	int					pokeCursor;
	int                 numberOfBlocks;
    size_t				eachBlockSize;
    size_t				lastBlockSize;
    int					buffersPerBlock;
	void				**blocks;
	int					ids[];
} chain_t;

typedef struct buffer {
#ifdef BUFFERS_EYECATCHER
	char				eyeCatcher[EYECATCHER_SIZE];
#endif
	int					bufferId;
	struct chain		*chain;
	struct buffer		*next;
	int					bufferSize;
	int					pilotSize;
	int					dataSize;
	char				*data;
	char				*cursor;
} buffer_t;

struct pool *
initBufferPool(int numberOfChains);

int
initBufferChain(
	struct pool	*pool,
	int			chainId,
	int			bufferSize,
	int			pilotSize,
	int			numberOfBuffers);

struct buffer *
bufferById(struct pool *pool, int chainId, int bufferId);

struct buffer *
peekBuffer(struct pool *pool);

struct buffer *
peekBufferOfSize(struct pool *pool, int preferredSize);

struct buffer *
peekBufferFromChain(struct pool *pool, int chainId);

void
pokeBuffer(struct buffer *buffer);

struct buffer *
nextBuffer(struct buffer *buffer);

struct buffer *
lastBuffer(struct buffer *buffer);

struct buffer *
appendBuffer(struct buffer *destination, struct buffer *appendage);

int
copyBuffer(struct buffer *destination, struct buffer *source);

int
totalDataSize(struct buffer *buffer);

void
resetBufferData(struct buffer *buffer, int leavePilot);

void
resetCursor(struct buffer *buffer, int leavePilot);

struct buffer *
putData(struct buffer *buffer, char *sourceData, int sourceDataSize);

struct buffer *
getData(struct buffer *buffer, char *destData, int destDataSize);

inline struct buffer *
putUInt8(struct buffer *buffer, uint8 sourceData);

inline struct buffer *
putUInt32(struct buffer *buffer, uint32 *sourceData);

inline struct buffer *
getUInt32(struct buffer *buffer, uint32 *destData);

inline struct buffer *
putString(struct buffer *buffer, char *sourceData, int sourceDataSize);

inline void
booleanInternetToPostgres(char *value);

inline void
booleanPostgresToInternet(char *value);

inline int
isPostgresBooleanTrue(char *value);

inline int
isPostgresBooleanFalse(char *value);

int
buffersInUse(struct pool *pool, int chainId);

#endif
