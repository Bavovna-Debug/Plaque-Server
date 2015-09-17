#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "buffers.h"
#include "report.h"

#ifdef PGBGW
#include <postgres.h>
#endif

// BUFFERS_EYECATCHER
// BUFFERS_INIT_CHAIN
// BUFFERS_INIT_CHAIN_DEEP
// BUFFERS_PEEK_POKE

#define NOTHING			-1

struct pool *
initBufferPool(int numberOfChains)
{
	struct pool *pool;

	pool = malloc(sizeof(struct pool) + numberOfChains * sizeof(struct chain *));
	if (pool == NULL) {
    	reportError("Out of memory");
        return NULL;
   	}

#ifdef BUFFERS_EYECATCHER
	strncpy(pool->eyeCatcher, "<<<< POOL >>>>", EYECATCHER_SIZE);
#endif

	pool->numberOfChains = numberOfChains;

    return pool;
}

int
initBufferChain(
	struct pool	*pool,
	int			chainId,
	int			bufferSize,
	int			pilotSize,
	int			numberOfBuffers)
{
	size_t		    maxBlockSize;
    long long	    totalBlockSize;
    int			    numberOfBlocks;
    size_t		    eachBlockSize;
    size_t		    lastBlockSize;
    int			    buffersPerBlock;
    size_t		    chainAllocSize;

    struct chain    *chain;

    int				blockId;
    int				blockSize;
	void			*block;
	int				bufferIdSequential;
	int				bufferIdInBlock;
	struct buffer	*buffer;

#ifdef BUFFERS_INIT_CHAIN
    reportLog("Create chain %d with %d buffers of size %d",
    	chainId,
    	numberOfBuffers,
    	bufferSize);
#endif

	maxBlockSize = MAX_BLOCK_SIZE - MAX_BLOCK_SIZE % sizeof(struct buffer);

	buffersPerBlock = maxBlockSize / sizeof(struct buffer);

	totalBlockSize = (long)numberOfBuffers * (long)sizeof(struct buffer);
	if (totalBlockSize <= maxBlockSize) {
		numberOfBlocks = 1;
		eachBlockSize = totalBlockSize;
		lastBlockSize = 0;
	} else {
		if ((totalBlockSize % maxBlockSize) == 0) {
			numberOfBlocks = totalBlockSize / maxBlockSize;
			eachBlockSize = maxBlockSize;
			lastBlockSize = 0;
		} else {
			numberOfBlocks = (totalBlockSize / maxBlockSize) + 1;
			eachBlockSize = maxBlockSize;
			lastBlockSize = totalBlockSize % maxBlockSize;
		}
	}

#ifdef BUFFERS_INIT_CHAIN
	reportLog("... maxBlockSize=%lu buffersPerBlock=%d numberOfBlocks=%d eachBlockSize=%lu lastBlockSize=%lu",
		maxBlockSize, buffersPerBlock, numberOfBlocks, eachBlockSize, lastBlockSize);
#endif

	chainAllocSize = sizeof(struct chain);
	chainAllocSize += numberOfBuffers * sizeof(int);
	chainAllocSize += numberOfBlocks * sizeof(void *);

	chain = malloc(chainAllocSize);
	if (chain == NULL) {
        reportError("Out of memory");
        return BUFFERS_OUT_OF_MEMORY_CHAIN;
    }

#ifdef BUFFERS_EYECATCHER
	strncpy(chain->eyeCatcher, "<<<< CHAIN >>>>", EYECATCHER_SIZE);
#endif

	pthread_spin_init(&chain->lock, PTHREAD_PROCESS_PRIVATE);

	chain->chainId			= chainId;
	chain->numberOfBuffers	= numberOfBuffers;
	chain->bufferSize		= bufferSize;
	chain->pilotSize		= pilotSize;
	chain->numberOfBlocks	= numberOfBlocks;
	chain->eachBlockSize	= eachBlockSize;
	chain->lastBlockSize	= lastBlockSize;
	chain->buffersPerBlock	= buffersPerBlock;
	chain->peekCursor		= 0;
	chain->pokeCursor		= 0;

	chain->blocks = (void *)((unsigned long)chain + (unsigned long)(chainAllocSize - numberOfBlocks * sizeof(void *)));

#ifdef BUFFERS_INIT_CHAIN
	reportLog("... chain=0x%08lX (%lu bytes) chain->blocks=0x%08lX",
	    (unsigned long)chain,
	    chainAllocSize,
	    (unsigned long)chain->blocks);
#endif

	blockId = 0;
	bufferIdSequential = 0;
	bufferIdInBlock = buffersPerBlock;

	while (bufferIdSequential < chain->numberOfBuffers)
	{
		if (bufferIdInBlock == buffersPerBlock) {
			bufferIdInBlock = 0;

			if ((blockId + 1) < numberOfBlocks) {
				blockSize = eachBlockSize;
			} else {
				blockSize = (lastBlockSize == 0) ? eachBlockSize : lastBlockSize;
			}

	    	block = malloc(blockSize);
			if (block == NULL) {
        		reportError("Out of memory");
    	    	return BUFFERS_OUT_OF_MEMORY_BLOCK;
		    }

#ifdef BUFFERS_INIT_CHAIN_DEEP
			reportLog("  > allocated block=0x%08lX",
			    (unsigned long)block);
#endif

			chain->blocks[blockId] = block;
			blockId++;
		}

		chain->ids[bufferIdSequential] = bufferIdSequential;

		buffer = (void *)((unsigned long)block + (unsigned long)(bufferIdInBlock * sizeof(struct buffer)));

#ifdef BUFFERS_EYECATCHER
		strncpy(buffer->eyeCatcher, "<<<< BUFFER >>>>", EYECATCHER_SIZE);
#endif

    	buffer->data = malloc(bufferSize);
		if (buffer->data == NULL) {
        	reportError("Out of memory");
    	    return BUFFERS_OUT_OF_MEMORY_DATA;
	    }

#ifdef BUFFERS_INIT_CHAIN_DEEP
		reportLog("    > buffer=0x%08lX buffer->data=0x%08lX",
		    (unsigned long)buffer,
		    (unsigned long)buffer->data);
#endif

		buffer->bufferId	= bufferIdSequential;
		buffer->chain		= chain;
		buffer->next		= NULL;
		buffer->bufferSize	= bufferSize;
		buffer->pilotSize	= pilotSize;
		buffer->dataSize	= 0;
		buffer->cursor		= buffer->data;

		bufferIdSequential++;
		bufferIdInBlock++;
	}

	pool->chains[chainId] = chain;

#ifdef BUFFERS_INIT_CHAIN
    reportLog("Created chain %d with %d buffers of size %d, with total chain size %lld MB",
    	chainId,
    	chain->numberOfBuffers,
    	bufferSize,
    	totalBlockSize >> 20);

    reportLog("There are %d block(s) of size %lu KB each and %lu KB last",
    	chain->numberOfBlocks,
    	chain->eachBlockSize >> 10,
    	chain->lastBlockSize >> 10);
#endif

	return 0;
}

struct buffer *
bufferById(struct pool *pool, int chainId, int bufferId)
{
    struct chain    *chain;
    void            *block;
    int             blockId;
    int             bufferIdInBlock;
	struct buffer   *buffer;

	chain = pool->chains[chainId];

	blockId = bufferId / chain->buffersPerBlock;
	bufferIdInBlock = bufferId % chain->buffersPerBlock;
	block = chain->blocks[blockId];

	buffer = (void *)((unsigned long)block + (unsigned long)(bufferIdInBlock * sizeof(struct buffer)));

#ifdef BUFFERS_PEEK_POKE
		reportLog("... bufferId=%d bufferIdInBlock=%d block=0x%08lX buffer=0x%08lX data=0x%08lX",
			bufferId,
			bufferIdInBlock,
			(unsigned long)block,
			(unsigned long)buffer,
			(unsigned long)buffer->data);
#endif

	return buffer;
}

struct buffer *
peekBuffer(struct pool *pool)
{
	return peekBufferFromChain(pool, 0);
}

struct buffer *
peekBufferOfSize(struct pool *pool, int preferredSize)
{
	struct chain *chain;
	struct buffer *buffer;
	int chainId;

#ifdef BUFFERS_PEEK_POKE
	reportLog("Peek buffer of size %d", preferredSize);
#endif

	buffer = NULL;
	chainId = 0;

	// Go through all chains, assuming chains are ordered by buffer size.
	//
	while (chainId < pool->numberOfChains)
	{
		chain = pool->chains[chainId];

		if (chain->bufferSize >= preferredSize) {
			buffer = peekBufferFromChain(pool, chainId);
			break;
		}

		chainId++;
	}

	// If preferred buffer size is greater than any buffer in any chain
	// then just take the biggest buffer - the one from the last chain.
	//
	if (buffer == NULL) {
#ifdef BUFFERS_PEEK_POKE
		reportLog("No free buffer for preferred size %d, take the biggest one", preferredSize);
#endif
		buffer = peekBufferFromChain(pool, pool->numberOfChains - 1);
	}

	if (buffer == NULL)
		reportLog("No free buffer available");

	return buffer;
}

struct buffer *
peekBufferFromChain(struct pool *pool, int chainId)
{
    struct chain    *chain;
	struct buffer   *buffer;
    int             bufferId;

#ifdef BUFFERS_PEEK_POKE
	reportLog("Peek buffer from chain %d", chainId);
#endif

	chain = pool->chains[chainId];

	pthread_spin_lock(&chain->lock);

	bufferId = chain->ids[chain->peekCursor];
#ifdef BUFFERS_PEEK_POKE
		reportLog("... chain->peekCursor=%d chain->pokeCursor=%d bufferId=%d",
			chain->peekCursor, chain->pokeCursor, bufferId);
#endif
	if (bufferId == NOTHING) {
		buffer = NULL;
	} else {
	    buffer = bufferById(pool, chainId, bufferId);

		chain->ids[chain->peekCursor] = NOTHING;

		chain->peekCursor++;
		if (chain->peekCursor == chain->numberOfBuffers)
			chain->peekCursor = 0;
	}

	pthread_spin_unlock(&chain->lock);

	if (buffer == NULL)
		reportLog("No free buffer available");

	return buffer;
}

void
pokeBuffer(struct buffer *buffer)
{
    struct chain    *chain;
    struct buffer   *nextBuffer;

#ifdef BUFFERS_PEEK_POKE
	reportLog("Poke buffer %d to chain %d", buffer->bufferId, buffer->chain->chainId);
#endif

	do {
		nextBuffer = buffer->next;

		buffer->next = NULL;
		buffer->dataSize = 0;
		buffer->cursor = buffer->data;

		chain = buffer->chain;

		pthread_spin_lock(&chain->lock);

		chain->ids[chain->pokeCursor] = buffer->bufferId;

		chain->pokeCursor++;
		if (chain->pokeCursor == chain->numberOfBuffers)
			chain->pokeCursor = 0;

		pthread_spin_unlock(&chain->lock);

		buffer = nextBuffer;

#ifdef BUFFERS_PEEK_POKE
		if (buffer != NULL)
			reportLog("... next buffer %d", buffer->bufferId);
#endif
	} while (buffer != NULL);
}

struct buffer *
nextBuffer(struct buffer *buffer)
{
	return buffer->next;
}

struct buffer *
lastBuffer(struct buffer *buffer)
{
    struct buffer *lastBuffer;

    lastBuffer = buffer;

    while (lastBuffer->next != NULL)
        lastBuffer = lastBuffer->next;

    return lastBuffer;
}

struct buffer *
appendBuffer(struct buffer *destination, struct buffer *appendage)
{
    struct buffer *buffer;

    if (destination == NULL) {
        return appendage;
    } else {
        buffer = lastBuffer(destination);
        buffer->next = appendage;
        return destination;
    }
}

int
copyBuffer(struct buffer *destination, struct buffer *source)
{
/*
    struct buffer *destinationBuffer;
    struct buffer *sourceBuffer;
    int bytesToCopy;

    destinationBuffer = destication;
    sourceBuffer = source;
    bytesToCopy = source->dataSize;
*/

    memcpy(destination->data, source->data, source->bufferSize);

    return source->bufferSize;
}

int
totalDataSize(struct buffer *firstBuffer)
{
	int totalDataSize = 0;
	struct buffer *buffer;
	for (buffer = firstBuffer; buffer != NULL; buffer = buffer->next)
		totalDataSize += buffer->dataSize;

	return totalDataSize;
}

void
resetBufferData(struct buffer *buffer, int leavePilot)
{
	if (leavePilot != 0) {
		buffer->dataSize = buffer->pilotSize;
		buffer = buffer->next;
	}

	while (buffer != NULL)
	{
		buffer->dataSize = 0;
		buffer = buffer->next;
	}
}

void
resetCursor(struct buffer *buffer, int leavePilot)
{
	if (leavePilot != 0) {
		buffer->cursor = buffer->data + buffer->pilotSize;
		buffer = buffer->next;
	}

	while (buffer != NULL)
	{
		buffer->cursor = buffer->data;
		buffer = buffer->next;
	}
}

struct buffer *
putData(struct buffer *buffer, char *sourceData, int sourceDataSize)
{
	int sourceDataOffset = 0;
	while (sourceDataOffset < sourceDataSize)
	{
		int freeSpaceInBuffer = buffer->bufferSize - buffer->dataSize;

		int bytesPerBuffer = sourceDataSize - sourceDataOffset;
		if (bytesPerBuffer > freeSpaceInBuffer)
			bytesPerBuffer = freeSpaceInBuffer;

		memcpy(buffer->data + buffer->dataSize, sourceData + sourceDataOffset, bytesPerBuffer);

		buffer->dataSize += bytesPerBuffer;

		sourceDataOffset += bytesPerBuffer;

		if (sourceDataOffset < sourceDataSize) {
			if (buffer->next == NULL) {
				buffer->next = peekBufferOfSize(buffer->chain->pool, buffer->bufferSize);
				if (buffer->next == NULL)
					return NULL;
			}

			buffer = buffer->next;
		}
	}

	return buffer;
}

struct buffer *
getData(struct buffer *buffer, char *destData, int destDataSize)
{
	int destDataOffset = 0;
	while (destDataOffset < destDataSize)
	{
		int leftSpaceInBuffer = buffer->bufferSize - (buffer->cursor - buffer->data);

		int bytesPerBuffer = destDataSize - destDataOffset;
		if (bytesPerBuffer > leftSpaceInBuffer)
			bytesPerBuffer = leftSpaceInBuffer;

		memcpy(destData + destDataOffset, buffer->cursor, bytesPerBuffer);

		buffer->cursor += bytesPerBuffer;

		destDataOffset += bytesPerBuffer;

		if (destDataOffset < destDataSize) {
			if (buffer->next == NULL)
				return NULL;

			buffer = buffer->next;
		}
	}

	return buffer;
}

inline struct buffer *
putUInt8(struct buffer *buffer, uint8_t sourceData)
{
	uint8_t value = sourceData;
	buffer = putData(buffer, (char *)&value, sizeof(value));
	return buffer;
}

inline struct buffer *
putUInt32(struct buffer *buffer, uint32_t *sourceData)
{
	uint32_t value = htobe32(*sourceData);
	buffer = putData(buffer, (char *)&value, sizeof(value));
	return buffer;
}

inline struct buffer *
getUInt32(struct buffer *buffer, uint32_t *destData)
{
	uint32_t value;
	buffer = getData(buffer, (char *)&value, sizeof(value));
	*destData = be32toh(value);
	return buffer;
}

inline struct buffer *
putString(struct buffer *buffer, char *sourceData, int sourceDataSize)
{
	uint32_t swapped = htobe32(sourceDataSize);
	buffer = putData(buffer, (char *)&swapped, sizeof(swapped));
	if (sourceDataSize != 0)
		buffer = putData(buffer, sourceData, sourceDataSize);
	return buffer;
}

inline void
booleanInternetToPostgres(char *value)
{
	*value = (*value == '0') ? 'f' : 't';
}

inline void
booleanPostgresToInternet(char *value)
{
	*value = (*value == 'f') ? '0' : '1';
}

inline int
isPostgresBooleanTrue(char *value)
{
	return (*value == 't');
}

inline int
isPostgresBooleanFalse(char *value)
{
	return (*value == 'f');
}

int
buffersInUse(struct pool *pool, int chainId)
{
	struct chain *chain;
	int buffersInUse;

	chain = pool->chains[chainId];
	pthread_spin_lock(&chain->lock);
	buffersInUse = (chain->peekCursor >= chain->pokeCursor)
		? chain->peekCursor - chain->pokeCursor
		: chain->numberOfBuffers - (chain->pokeCursor - chain->peekCursor);
	pthread_spin_unlock(&chain->lock);
	return buffersInUse;
}
