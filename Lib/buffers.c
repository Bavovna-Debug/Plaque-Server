#include <c.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "buffers.h"
#include "report.h"

#ifdef PGBGW
#include <postgres.h>
#endif

/**********************************************************************************/
/*                                                                                */
/*  Debug compile options:                                                        */
/*      BUFFERS_EYECATCHER                                                        */
/*      BUFFERS_INIT_BANK                                                         */
/*      BUFFERS_INIT_BANK_DEEP                                                    */
/*      BUFFERS_PEEK_POKE                                                         */
/*                                                                                */
/**********************************************************************************/

#define NOTHING -1

static inline struct buffer *
bufferById(struct pool *pool, unsigned int bankId, unsigned int bufferId);

/**
 * initBufferPool - Initialize buffer pool.
 * @numberOfBanks: Number of banks to allocate. Each bank has to be initialized with initBufferBank().
 * Returns buffer pool upon successful completion or NULL if buffer pool cannot be created.
 */
struct pool *
initBufferPool(unsigned int numberOfBanks)
{
	struct pool *pool;

	pool = malloc(sizeof(struct pool) + numberOfBanks * sizeof(struct bank *));
	if (pool == NULL) {
    	reportError("Out of memory");
        return NULL;
   	}

#ifdef BUFFERS_EYECATCHER
	strncpy(pool->eyeCatcher, "<<<< POOL >>>>", EYECATCHER_SIZE);
#endif

	pool->numberOfBanks = numberOfBanks;

    return pool;
}

/**
 * initBufferBank - Initialize buffer bank (buffer pool must be already initialized).
 * @pool: Buffer pool (must be already initialized).
 * @bankId: Bank id.
 * @bufferSize: Buffer size.
 * @pilotSize: Size of pilot data (0 if data in this bank does not suppose to have pilot).
 * @numberOfBuffers: Number of buffers.
 * Returns 0 upon successful completion or error code if buffer bank cannot be created.
 */
int
initBufferBank(
	struct pool     *pool,
	unsigned int    bankId,
	unsigned int    bufferSize,
	unsigned int    pilotSize,
	unsigned int    numberOfBuffers)
{
	size_t		    maxBlockSize;
    long long	    totalBlockSize;
    unsigned int    numberOfBlocks;
    size_t		    eachBlockSize;
    size_t		    lastBlockSize;
    unsigned int    buffersPerBlock;
    size_t		    bankAllocSize;

    struct bank     *bank;

    unsigned int    blockId;
    unsigned int    blockSize;
	void			*block;
	unsigned int    bufferIdSequential;
	unsigned int    bufferIdInBlock;
	struct buffer	*buffer;

#ifdef BUFFERS_INIT_CHAIN
    reportLog("Create bank %d with %d buffers of size %d",
    	bankId,
    	numberOfBuffers,
    	bufferSize);
#endif

    if ((bankId + 1) > pool->numberOfBanks)
        return -1;

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

    // Calculate amount of memory to be allocated for the bank.
    //
	bankAllocSize = sizeof(struct bank);
	bankAllocSize += numberOfBuffers * sizeof(int);
	bankAllocSize += numberOfBlocks * sizeof(void *);

	bank = malloc(bankAllocSize);
	if (bank == NULL) {
        reportError("Out of memory");
        return BUFFERS_OUT_OF_MEMORY_CHAIN;
    }

#ifdef BUFFERS_EYECATCHER
	strncpy(bank->eyeCatcher, "<<<< CHAIN >>>>", EYECATCHER_SIZE);
#endif

    // Initialize bank values.

	pthread_spin_init(&bank->lock, PTHREAD_PROCESS_PRIVATE);

	bank->bankId			= bankId;
	bank->pool              = pool;
	bank->numberOfBuffers	= numberOfBuffers;
	bank->bufferSize		= bufferSize;
	bank->pilotSize         = pilotSize;
	bank->numberOfBlocks	= numberOfBlocks;
	bank->eachBlockSize     = eachBlockSize;
	bank->lastBlockSize     = lastBlockSize;
	bank->buffersPerBlock	= buffersPerBlock;
	bank->peekCursor		= 0;
	bank->pokeCursor		= 0;

	bank->blocks = (void *)((unsigned long)bank + (unsigned long)(bankAllocSize - numberOfBlocks * sizeof(void *)));

#ifdef BUFFERS_INIT_CHAIN
	reportLog("... bank=0x%08lX (%lu bytes) bank->blocks=0x%08lX",
	    (unsigned long)bank,
	    bankAllocSize,
	    (unsigned long)bank->blocks);
#endif

	blockId = 0;
	bufferIdSequential = 0;
	bufferIdInBlock = buffersPerBlock;

	while (bufferIdSequential < bank->numberOfBuffers)
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

			bank->blocks[blockId] = block;
			blockId++;
		}

		bank->ids[bufferIdSequential] = bufferIdSequential;

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
		buffer->bank		= bank;
		buffer->prev		= NULL;
		buffer->next		= NULL;
		buffer->bufferSize	= bufferSize;
		buffer->pilotSize	= pilotSize;
		buffer->dataSize	= 0;
		buffer->userId      = 0;
		buffer->cursor		= buffer->data;

		bufferIdSequential++;
		bufferIdInBlock++;
	}

	pool->banks[bankId] = bank;

#ifdef BUFFERS_INIT_CHAIN
    reportLog("Created bank %d with %d buffers of size %d, with total bank size %lld MB",
    	bankId,
    	bank->numberOfBuffers,
    	bufferSize,
    	totalBlockSize >> 20);

    reportLog("There are %d block(s) of size %lu KB each and %lu KB last",
    	bank->numberOfBlocks,
    	bank->eachBlockSize >> 10,
    	bank->lastBlockSize >> 10);
#endif

	return 0;
}

/**
 * bufferById - Get buffer for specified buffer pool and buffer bank.
 * @pool: Buffer pool.
 * @bankId: Buffer bank id.
 * @bufferId: Buffer id.
 * Returns buffer.
 */
static inline struct buffer *
bufferById(struct pool *pool, unsigned int bankId, unsigned int bufferId)
{
    struct bank     *bank;
    void            *block;
    unsigned int    blockId;
    unsigned int    bufferIdInBlock;
	struct buffer   *buffer;

	bank = pool->banks[bankId];

	blockId = bufferId / bank->buffersPerBlock;
	bufferIdInBlock = bufferId % bank->buffersPerBlock;
	block = bank->blocks[blockId];

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

/**
 * peekBuffer - Peek buffer from the first bank of the buffer pool.
 * @pool: Buffer pool.
 * Returns buffer or NULL if there are no buffers available in a specified pool.
 */
struct buffer *
peekBuffer(struct pool *pool, unsigned int userId)
{
	return peekBufferFromBank(pool, 0, userId);
}

/**
 * peekBufferOfSize - Peek buffer from of the specified preferred size.
 *                    Go through all banks until free buffer is found.
 * @pool: Buffer pool.
 * @preferredSize: Preferred/minimum size of requested buffer.
 * Returns buffer or NULL if there are no buffers available with specified preferred size.
 */
struct buffer *
peekBufferOfSize(struct pool *pool, unsigned int preferredSize, unsigned int userId)
{
	struct bank     *bank;
	struct buffer   *buffer;
	unsigned int    bankId;

#ifdef BUFFERS_PEEK_POKE
	reportLog("Peek buffer of size %d for 0x%08X", preferredSize, userId);
#endif

	buffer = NULL;
	bankId = 0;

	// Go through all banks, assuming banks are ordered by buffer size.
	//
	while (bankId < pool->numberOfBanks)
	{
		bank = pool->banks[bankId];

        // Ich buffer size of this bank fits to request ...
        //
		if (bank->bufferSize >= preferredSize) {
		    //
		    // ... then try to get a buffer from this bank.
		    //
			buffer = peekBufferFromBank(pool, bankId, userId);

			if (buffer != NULL)
    			break;

			// If there are no free buffers in this bank, then go long with further cheins.
		}

		bankId++;
	}

	if (buffer == NULL)
		reportLog("No free buffer available");

	return buffer;
}

/**
 * peekBufferFromBank - Peek buffer from the specified bank.
 * @pool: Buffer pool.
 * @bankId: Bank id.
 * Returns buffer or NULL if there are no buffers available in a specified bank.
 */
struct buffer *
peekBufferFromBank(struct pool *pool, unsigned int bankId, unsigned int userId)
{
    struct bank     *bank;
	struct buffer   *buffer;
    unsigned int    bufferId;

#ifdef BUFFERS_PEEK_POKE
	reportLog("Peek buffer from bank %d for 0x%08X", bankId, userId);
#endif

	bank = pool->banks[bankId];

	pthread_spin_lock(&bank->lock);

	bufferId = bank->ids[bank->peekCursor];
#ifdef BUFFERS_PEEK_POKE
		reportLog("... bank->peekCursor=%d bank->pokeCursor=%d bufferId=%d",
			bank->peekCursor, bank->pokeCursor, bufferId);
#endif
	if (bufferId == NOTHING) {
	    //
	    // All buffers are in use (peek cursor has reached the poke cursor
	    // and there are no buffers behind the poke cursor in the buffer bank).
	    //
		buffer = NULL;
	} else {
	    buffer = bufferById(pool, bankId, bufferId);

	    if (buffer->userId != 0) {
        	pthread_spin_unlock(&bank->lock);
	        reportError("Got buffer that already belongs to 0x%08X", userId);
	        return NULL;
	    }

	    buffer->userId = userId;

		bank->ids[bank->peekCursor] = NOTHING;

		bank->peekCursor++;
		if (bank->peekCursor == bank->numberOfBuffers)
			bank->peekCursor = 0;
	}

	pthread_spin_unlock(&bank->lock);

	if (buffer == NULL)
		reportLog("No free buffer available");

	return buffer;
}

/**
 * pokeBuffer - Poke buffer back to its bank.
 * @buffer: Buffer.
 */
void
pokeBuffer(struct buffer *buffer)
{
    struct bank     *bank;
    struct buffer   *thisBuffer;
    struct buffer   *nextBuffer;

#ifdef BUFFERS_PEEK_POKE
	reportLog("Poke buffer %d to bank %d of chain %s for 0x%08X",
	    buffer->bufferId, buffer->bank->bankId, buffer->userId);
#endif

	pthread_spin_lock(&bank->lock);

	thisBuffer = firstBuffer(buffer);
	do {
	    if (thisBuffer->userId == 0) {
	        reportError("Trying to poke buffer %d to bank %d that was not peeked",
	            thisBuffer->bufferId, thisBuffer->bank->bankId);
	        break;
	    }

reportLog("#1");
		nextBuffer = thisBuffer->next;

        if ((nextBuffer != NULL) && (nextBuffer->prev != thisBuffer))
            nextBuffer = NULL;

		thisBuffer->prev = NULL;
		thisBuffer->next = NULL;
		thisBuffer->dataSize = 0;
		thisBuffer->cursor = thisBuffer->data;
		thisBuffer->userId = 0;

		bank = thisBuffer->bank;

reportLog("#2");
		bank->ids[bank->pokeCursor] = thisBuffer->bufferId;
reportLog("#3");

		bank->pokeCursor++;
		if (bank->pokeCursor == bank->numberOfBuffers)
			bank->pokeCursor = 0;

		thisBuffer = nextBuffer;

#ifdef BUFFERS_PEEK_POKE
		if (thisBuffer != NULL)
			reportLog("... next buffer %d", thisBuffer->bufferId);
#endif
reportLog("#4");
	} while (thisBuffer != NULL);
reportLog("#5");

	pthread_spin_unlock(&bank->lock);
reportLog("#6");
}

/**
 * previousBuffer - Get previous buffer in a chain.
 * @buffer: Buffer.
 * Returns previous buffer.
 */
inline struct buffer *
previousBuffer(struct buffer *buffer)
{
	return buffer->prev;
}

/**
 * nextBuffer - Get next buffer in a chain.
 * @buffer: Buffer.
 * Returns next buffer.
 */
inline struct buffer *
nextBuffer(struct buffer *buffer)
{
	return buffer->next;
}

/**
 * firstBuffer - Get first buffer in a chain.
 * @buffer: Buffer.
 * Returns first buffer.
 */
inline struct buffer *
firstBuffer(struct buffer *buffer)
{
    struct buffer *firstBuffer;

    firstBuffer = buffer;

    while (firstBuffer->prev != NULL)
        firstBuffer = firstBuffer->prev;

    return firstBuffer;
}

/**
 * lastBuffer - Get last buffer in a chain.
 * @buffer: Buffer.
 * Returns last buffer.
 */
inline struct buffer *
lastBuffer(struct buffer *buffer)
{
    struct buffer *lastBuffer;

    lastBuffer = buffer;

    while (lastBuffer->next != NULL)
        lastBuffer = lastBuffer->next;

    return lastBuffer;
}

/**
 * appendBuffer - Append one more buffer at the end of a specified buffer chain.
 * @destination: Buffer chain to append to.
 * @appendage: Buffer or buffer chain to be appended.
 * Returns anchor of a buffer chain.
 */
inline struct buffer *
extendBuffer(struct buffer *buffer)
{
    return peekBufferOfSize(buffer->bank->pool, buffer->bufferSize, buffer->userId);
}

/**
 * appendBuffer - Append one more buffer at the end of a specified buffer chain.
 * @destination: Buffer chain to append to.
 * @appendage: Buffer or buffer chain to be appended.
 * Returns anchor of a buffer chain.
 */
inline struct buffer *
appendBuffer(struct buffer *destination, struct buffer *appendage)
{
    struct buffer *buffer;

    if (destination == NULL) {
        return appendage;
    } else {
        buffer = lastBuffer(destination);
        buffer->next = appendage;
        appendage->prev = buffer;
        return destination;
    }
}

/**
 * copyBuffer - Copy data from one buffer chain to another. Extend destination buffer chain if necessary.
 * @destination: Destination buffer chain.
 * @source: Source buffer chain.
 * Returns amount of data copied.
 */
unsigned int
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

/**
 * totalDataSize - Get total data size summarized for all buffers in a chain.
 * @firstBuffer: Buffer in a chain to begin counting from.
 * Returns total data size.
 */
unsigned int
totalDataSize(struct buffer *firstBuffer)
{
	unsigned int totalDataSize = 0;
	struct buffer *buffer;
	for (buffer = firstBuffer; buffer != NULL; buffer = buffer->next)
		totalDataSize += buffer->dataSize;

	return totalDataSize;
}

/**
 * resetBufferData - Reset data size of the buffer chain.
 * @buffer: Anchor buffer of a chain.
 * @leavePilot: Boolean value specifying whether pilot has to be left in the anchor buffer.
 */
void
resetBufferData(struct buffer *buffer, int leavePilot)
{
    // Reset data size of the first buffer in a chain. Leave pilot if needed.
    //
	if (leavePilot != 0) {
		buffer->dataSize = buffer->pilotSize;
		buffer = buffer->next;
	}

    // Reset data size of the rest buffers in a chain.
    //
	while (buffer != NULL)
	{
		buffer->dataSize = 0;
		buffer = buffer->next;
	}
}

/**
 * resetCursor - Reset cursor.
 * @buffer: Anchor buffer of a chain.
 * @leavePilot: Boolean value specifying whether cursor must point to a beginning of buffer
 *              or to the end of pilot.
 */
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

/**
 * putData - Put (write) data to a buffer chain under cursor. Append new buffer if necessary.
 * @buffer: Buffer.
 * @sourceData: Pointer to data.
 * @sourceDataSize: Size of data.
 * Returns buffer where cursor is pointing to after write is complete.
 */
struct buffer *
putData(struct buffer *buffer, char *sourceData, unsigned int sourceDataSize)
{
	unsigned int sourceDataOffset = 0;
	while (sourceDataOffset < sourceDataSize)
	{
		int freeSpaceInBuffer = buffer->bufferSize - buffer->dataSize;

        // Calculate the amount of data to be written to the current buffer.
        //
		int bytesPerBuffer = sourceDataSize - sourceDataOffset;
		if (bytesPerBuffer > freeSpaceInBuffer)
			bytesPerBuffer = freeSpaceInBuffer;

        // Copy data or a portion of data that may fit in current buffer.
        //
		memcpy(buffer->data + buffer->dataSize, sourceData + sourceDataOffset, bytesPerBuffer);

		buffer->dataSize += bytesPerBuffer;

		sourceDataOffset += bytesPerBuffer;

        // If free space in current buffer was not enough to write complete data ...
        //
		if (sourceDataOffset < sourceDataSize) {
		    //
		    // ... then switch to next buffer in a chain or append a new buffer if necessary.
		    //
			if (buffer->next != NULL) {
			    //
                // Switch to next buffer.
                //
    			buffer = buffer->next;
    		} else {
				buffer = extendBuffer(buffer);
				if (buffer == NULL)
					return NULL;
			}
		}
	}

	return buffer;
}

/**
 * getData - Get (read) data from a buffer chain under cursor.
 * @buffer: Buffer.
 * @destData: Pointer to memory area to copy data to.
 * @destDataSize: Size of data to copy.
 * Returns buffer where cursor is pointing to after read is complete.
 */
struct buffer *
getData(struct buffer *buffer, char *destData, unsigned int destDataSize)
{
	unsigned int destDataOffset = 0;
	while (destDataOffset < destDataSize)
	{
		int leftSpaceInBuffer = buffer->bufferSize - (buffer->cursor - buffer->data);

        // Calculate the amount of data to be read from the current buffer.
        //
		int bytesPerBuffer = destDataSize - destDataOffset;
		if (bytesPerBuffer > leftSpaceInBuffer)
			bytesPerBuffer = leftSpaceInBuffer;

        // Copy data or a portion of data from current buffer.
        //
		memcpy(destData + destDataOffset, buffer->cursor, bytesPerBuffer);

		buffer->cursor += bytesPerBuffer;

		destDataOffset += bytesPerBuffer;

        // In case more data has to be read from buffer chain ...
        //
		if (destDataOffset < destDataSize) {
		    //
		    // ... then switch to next buffer in a chain.
		    // Quit, if not all data was read and there are no other buffers in a chain.
		    //
			if (buffer->next == NULL)
				return NULL;

            // Switch to next buffer.
            //
			buffer = buffer->next;
		}
	}

	return buffer;
}

/**
 * putUInt8 - Put one byte to buffer chain under cursor.
 * @buffer: Buffer.
 * @sourceData: Pointer to data.
 * Returns buffer where cursor is pointing to after write is complete.
 */
inline struct buffer *
putUInt8(struct buffer *buffer, uint8 *sourceData)
{
	buffer = putData(buffer, (char *)sourceData, sizeof(uint8));
	return buffer;
}

/**
 * getUInt8 - Get one bytes from buffer chain under cursor.
 * @buffer: Buffer.
 * @destData: Pointer to memory area to copy data to.
 * Returns buffer where cursor is pointing to after read is complete.
 */
inline struct buffer *
getUInt8(struct buffer *buffer, uint8 *destData)
{
	buffer = getData(buffer, (char *)destData, sizeof(uint8));
	return buffer;
}

/**
 * putUInt16 - Put two bytes integer to buffer chain under cursor.
 * @buffer: Buffer.
 * @sourceData: Pointer to data.
 * Returns buffer where cursor is pointing to after write is complete.
 */
inline struct buffer *
putUInt16(struct buffer *buffer, uint16 *sourceData)
{
	uint16 value = htobe16(*sourceData);
	buffer = putData(buffer, (char *)&value, sizeof(value));
	return buffer;
}

/**
 * getUInt16 - Get two bytes integer from buffer chain under cursor.
 * @buffer: Buffer.
 * @destData: Pointer to memory area to copy data to.
 * Returns buffer where cursor is pointing to after read is complete.
 */
inline struct buffer *
getUInt16(struct buffer *buffer, uint16 *destData)
{
	uint16 value;
	buffer = getData(buffer, (char *)&value, sizeof(value));
	*destData = be16toh(value);
	return buffer;
}

/**
 * putUInt32 - Put four bytes integer to buffer chain under cursor.
 * @buffer: Buffer.
 * @sourceData: Pointer to data.
 * Returns buffer where cursor is pointing to after write is complete.
 */
inline struct buffer *
putUInt32(struct buffer *buffer, uint32 *sourceData)
{
	uint32 value = htobe32(*sourceData);
	buffer = putData(buffer, (char *)&value, sizeof(value));
	return buffer;
}

/**
 * getUInt32 - Get four bytes integer from buffer chain under cursor.
 * @buffer: Buffer.
 * @destData: Pointer to memory area to copy data to.
 * Returns buffer where cursor is pointing to after read is complete.
 */
inline struct buffer *
getUInt32(struct buffer *buffer, uint32 *destData)
{
	uint32 value;
	buffer = getData(buffer, (char *)&value, sizeof(value));
	*destData = be32toh(value);
	return buffer;
}

/**
 * putUInt64 - Put eight bytes integer to buffer chain under cursor.
 * @buffer: Buffer.
 * @sourceData: Pointer to data.
 * Returns buffer where cursor is pointing to after write is complete.
 */
inline struct buffer *
putUInt64(struct buffer *buffer, uint64 *sourceData)
{
	uint64 value = htobe64(*sourceData);
	buffer = putData(buffer, (char *)&value, sizeof(value));
	return buffer;
}

/**
 * getUInt64 - Get eight bytes integer from buffer chain under cursor.
 * @buffer: Buffer.
 * @destData: Pointer to memory area to copy data to.
 * Returns buffer where cursor is pointing to after read is complete.
 */
inline struct buffer *
getUInt64(struct buffer *buffer, uint64 *destData)
{
	uint64 value;
	buffer = getData(buffer, (char *)&value, sizeof(value));
	*destData = be64toh(value);
	return buffer;
}

/**
 * putString - Put four bytes integer to buffer chain under cursor.
 * @buffer: Buffer.
 * @sourceData: Pointer to data (beginning of a string).
 * @sourceDataSize: Size of data (size/length of a string).
 * Returns buffer where cursor is pointing to after write is complete.
 */
inline struct buffer *
putString(struct buffer *buffer, char *sourceData, unsigned int sourceDataSize)
{
	uint32 swapped = htobe32(sourceDataSize);
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

/**
 * buffersInUse - Get number of used buffers for specified buffer chain.
 * @pool: Buffer pool.
 * @bankId: Buffer bank id.
 * Returns the number of buffers 'in use'.
 */
unsigned int
buffersInUse(struct pool *pool, unsigned int bankId)
{
	struct bank *bank;
	unsigned int buffersInUse;

	bank = pool->banks[bankId];
	pthread_spin_lock(&bank->lock);
	buffersInUse = (bank->peekCursor >= bank->pokeCursor)
		? bank->peekCursor - bank->pokeCursor
		: bank->numberOfBuffers - (bank->pokeCursor - bank->peekCursor);
	pthread_spin_unlock(&bank->lock);
	return buffersInUse;
}
