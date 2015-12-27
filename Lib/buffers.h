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
	unsigned int		numberOfBanks;
	struct bank		    *banks[];
} pool_t;

typedef struct bank {
#ifdef BUFFERS_EYECATCHER
	char				eyeCatcher[EYECATCHER_SIZE];
#endif
	pthread_spinlock_t	lock;
	unsigned int		bankId;
	struct pool			*pool;
	unsigned int		numberOfBuffers;
	unsigned int		bufferSize;
	unsigned int		pilotSize;
	unsigned int		peekCursor;
	unsigned int		pokeCursor;
	unsigned int        numberOfBlocks;
    size_t				eachBlockSize;
    size_t				lastBlockSize;
    unsigned int		buffersPerBlock;
	void				**blocks;
	unsigned int		ids[];
} bank_t;

typedef struct buffer {
#ifdef BUFFERS_EYECATCHER
	char				eyeCatcher[EYECATCHER_SIZE];
#endif
	unsigned int		bufferId;
	struct bank		    *bank;
	struct buffer		*prev;
	struct buffer		*next;
	unsigned int		bufferSize;
	unsigned int		pilotSize;
	unsigned int		dataSize;
	unsigned int		userId;
	char				*data;
	char				*cursor;
} buffer_t;

struct pool *
initBufferPool(unsigned int numberOfBanks);

int
initBufferBank(
	struct pool     *pool,
	unsigned int    bankId,
	unsigned int    bufferSize,
	unsigned int    pilotSize,
	unsigned int    numberOfBuffers);

struct buffer *
peekBuffer(struct pool *pool, unsigned int userId);

struct buffer *
peekBufferOfSize(struct pool *pool, unsigned int preferredSize, unsigned int userId);

struct buffer *
peekBufferFromBank(struct pool *pool, unsigned int bankId, unsigned int userId);

void
pokeBuffer(struct buffer *buffer);

inline struct buffer *
previousBuffer(struct buffer *buffer);

inline struct buffer *
nextBuffer(struct buffer *buffer);

inline struct buffer *
firstBuffer(struct buffer *buffer);

inline struct buffer *
lastBuffer(struct buffer *buffer);

inline struct buffer *
extendBuffer(struct buffer *buffer);

inline struct buffer *
appendBuffer(struct buffer *destination, struct buffer *appendage);

unsigned int
copyBuffer(struct buffer *destination, struct buffer *source);

unsigned int
totalDataSize(struct buffer *buffer);

void
resetBufferData(struct buffer *buffer, int leavePilot);

void
resetCursor(struct buffer *buffer, int leavePilot);

struct buffer *
putData(struct buffer *buffer, char *sourceData, unsigned int sourceDataSize);

struct buffer *
getData(struct buffer *buffer, char *destData, unsigned int destDataSize);

inline struct buffer *
putUInt8(struct buffer *buffer, uint8 *sourceData);

inline struct buffer *
getUInt8(struct buffer *buffer, uint8 *destData);

inline struct buffer *
putUInt16(struct buffer *buffer, uint16 *sourceData);

inline struct buffer *
getUInt16(struct buffer *buffer, uint16 *destData);

inline struct buffer *
putUInt32(struct buffer *buffer, uint32 *sourceData);

inline struct buffer *
getUInt32(struct buffer *buffer, uint32 *destData);

inline struct buffer *
putUInt64(struct buffer *buffer, uint64 *sourceData);

inline struct buffer *
getUInt64(struct buffer *buffer, uint64 *destData);

inline struct buffer *
putString(struct buffer *buffer, char *sourceData, unsigned int sourceDataSize);

inline void
booleanInternetToPostgres(char *value);

inline void
booleanPostgresToInternet(char *value);

inline int
isPostgresBooleanTrue(char *value);

inline int
isPostgresBooleanFalse(char *value);

unsigned int
buffersInUse(struct pool *pool, unsigned int bankId);

#endif
