#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "mmps.h"
#include "report.h"

const unsigned int NOTHING = 0xFFFF0000;

/**
 * MMPS_InitPool()
 * Allocate and initialize buffer pool.
 *
 * @numberOfBanks: Number of banks to allocate. Each bank has to be initialized
 *                 afterwards using MMPS_InitBank().
 *
 * Returns pointer to MMPS pool handler upon successful completion or NULL,
 * if MMPS pool cannot be created.
 */
struct MMPS_Pool *
MMPS_InitPool(const unsigned int numberOfBanks)
{
    struct MMPS_Pool *pool;

    pool = malloc(sizeof(struct MMPS_Pool) + numberOfBanks * sizeof(struct MMPS_Bank *));
    if (pool == NULL)
    {
        ReportSoftAlert("Out of memory");

        return NULL;
    }

#ifdef MMPS_EYECATCHER
    strncpy(pool->eyeCatcher, EYECATCHER_POOL, EYECATCHER_SIZE);
#endif

    pool->numberOfBanks = numberOfBanks;

    // Make sure pointers to buffer banks are set to null.
    //
    bzero(&pool->banks, numberOfBanks * sizeof(struct MMPS_Bank *));

    return pool;
}

/**
 * MMPS_InitBank()
 * Initialize buffer bank - MMPS pool must be already initialized.
 * This routine allocates and initializes memory structures
 * necessary for bank handler and all buffers of the specified bank.
 *
 * @pool:               Pointer to MMPS pool handler - already initialized
 *                      with MMPS_InitPool().
 * @bankId:             Bank id of the bank to be initialized.
 * @bufferSize:         Value greater than 0 specifies the size of buffers in a bank.
 *                      All buffers in a bank will be created of the given size.
 *                      Value of 0 specifies that the specified bank will contain
 *                      buffers of different sizes. In such case MMPS will only
 *                      allocate buffer handlers but not data blocks. Data blocks
 *                      could be associated with buffer handlers later.
 * @pilotSize:          Size of pilot data or 0, if data in this bank is not supposed
 *                      to have pilot.
 * @numberOfBuffers:    Number of buffer handlers to be created in the specified bank.
 *                      Different banks in one MMPS pool may have different
 *                      number of buffers.
 *
 * Returns 0 upon successful completion or an error code (negative value),
 * if bank cannot be created.
 */
int
MMPS_InitBank(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    unsigned int        bufferSize,
    unsigned int        pilotSize,
    unsigned int        numberOfBuffers)
{
#ifdef MMPS_MUTEX
    pthread_mutexattr_t mutexAttr;
#endif
    size_t              maxBlockSize;
    long long           totalBlockSize;
    unsigned int        numberOfBlocks;
    size_t              eachBlockSize;
    size_t              lastBlockSize;
    unsigned int        buffersPerBlock;
    size_t              bankAllocSize;

    struct MMPS_Bank    *bank;
    struct MMPS_Buffer  *buffer;

    unsigned int        blockId;
    unsigned int        blockSize;
    void                *block;
    unsigned int        bufferIdSequential;
    unsigned int        bufferIdInBlock;

#ifdef MMPS_INIT_BANK
    ReportDebug("Create bank %u with %u buffers of size %u",
        bankId,
        numberOfBuffers,
        bufferSize);
#endif

    // Quit if bank id is out of range.
    //
    if ((bankId + 1) > pool->numberOfBanks)
        return MMPS_WRONG_BANK_ID;

    // Quit if buffer bank with specified id already exists.
    //
    if (pool->banks[bankId] != NULL)
        return MMPS_WRONG_BANK_ID;

    // Calculate parameters needed to allocate the memory structures
    // to store the buffer queue.

    maxBlockSize = MAX_BLOCK_SIZE - MAX_BLOCK_SIZE % sizeof(struct MMPS_Buffer);

    buffersPerBlock = maxBlockSize / sizeof(struct MMPS_Buffer);

    totalBlockSize = (long) numberOfBuffers * (long)sizeof(struct MMPS_Buffer);
    if (totalBlockSize <= maxBlockSize)
    {
        numberOfBlocks = 1;
        eachBlockSize = totalBlockSize;
        lastBlockSize = 0;
    }
    else
    {
        if ((totalBlockSize % maxBlockSize) == 0)
        {
            numberOfBlocks = totalBlockSize / maxBlockSize;
            eachBlockSize = maxBlockSize;
            lastBlockSize = 0;
        }
        else
        {
            numberOfBlocks = (totalBlockSize / maxBlockSize) + 1;
            eachBlockSize = maxBlockSize;
            lastBlockSize = totalBlockSize % maxBlockSize;
        }
    }

#ifdef MMPS_INIT_BANK
    ReportDebug("... maxBlockSize=%lu buffersPerBlock=%u numberOfBlocks=%u eachBlockSize=%lu lastBlockSize=%lu",
        maxBlockSize,
        buffersPerBlock,
        numberOfBlocks,
        eachBlockSize,
        lastBlockSize);
#endif

    // Calculate amount of memory to be allocated for the bank.
    //
    bankAllocSize = sizeof(struct MMPS_Bank);
    bankAllocSize += numberOfBuffers * sizeof(unsigned int);
    bankAllocSize += numberOfBlocks * sizeof(void *);

    bank = malloc(bankAllocSize);
    if (bank == NULL)
    {
        ReportSoftAlert("Out of memory");

        return MMPS_OUT_OF_MEMORY;
    }

#ifdef MMPS_EYECATCHER
    strncpy(bank->eyeCatcher, EYECATCHER_BANK, EYECATCHER_SIZE);
#endif

    // Initialize bank values.

#ifdef MMPS_MUTEX
    pthread_mutexattr_init(&mutexAttr);

    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_PRIVATE);

    pthread_mutex_init(&bank->lock, &mutexAttr);
#else
    pthread_spin_init(&bank->lock, PTHREAD_PROCESS_PRIVATE);
#endif

    bank->bankId            = bankId;
    bank->pool              = pool;
    bank->numberOfBuffers   = numberOfBuffers;
    bank->bufferSize        = bufferSize;
    bank->pilotSize         = pilotSize;
    bank->numberOfBlocks    = numberOfBlocks;
    bank->eachBlockSize     = eachBlockSize;
    bank->lastBlockSize     = lastBlockSize;
    bank->buffersPerBlock   = buffersPerBlock;
    bank->cursor.peek       = 0;
    bank->cursor.poke       = 0;

    bank->blocks = (void *) (
        (unsigned long) bank +
        (unsigned long) (bankAllocSize - numberOfBlocks * sizeof(void *))
    );

#ifdef MMPS_INIT_BANK
    ReportDebug("... bank=0x%08lX (%lu bytes) bank->blocks=0x%08lX",
        (unsigned long) bank,
        bankAllocSize,
        (unsigned long) bank->blocks);
#endif

    blockId = 0;
    bufferIdSequential = 0;
    bufferIdInBlock = buffersPerBlock;

    // Set block to null to omit compiler message 'block may be used uninitialized'.
    //
    block = NULL;

    // Allocate all parts of buffer queue, allocate all buffers and put them in a queue.
    //
    while (bufferIdSequential < bank->numberOfBuffers)
    {
        // Is it time to allocate new block for buffer queue?
        //
        if (bufferIdInBlock == buffersPerBlock)
        {
            bufferIdInBlock = 0;

            if ((blockId + 1) < numberOfBlocks) {
                blockSize = eachBlockSize;
            } else {
                blockSize = (lastBlockSize == 0) ? eachBlockSize : lastBlockSize;
            }

            block = malloc(blockSize);
            if (block == NULL)
            {
                ReportSoftAlert("Out of memory");

                return MMPS_OUT_OF_MEMORY;
            }

#ifdef MMPS_INIT_BANK_DEEP
            ReportDebug("  > allocated block=0x%08lX",
                (unsigned long) block);
#endif

            bank->blocks[blockId] = block;
            blockId++;
        }

        bank->ids[bufferIdSequential] = bufferIdSequential;

        buffer = (void *)
            ((unsigned long) block + (unsigned long) (bufferIdInBlock * sizeof(struct MMPS_Buffer)));

#ifdef MMPS_EYECATCHER
        strncpy(buffer->eyeCatcher, EYECATCHER_BUFFER, EYECATCHER_SIZE);
#endif

        // Allocate memory for a buffer.
        //
        buffer->data = malloc(bufferSize);
        if (buffer->data == NULL)
        {
            ReportSoftAlert("Out of memory");

            return MMPS_OUT_OF_MEMORY;
        }

#ifdef MMPS_INIT_BANK_DEEP
        ReportDebug("    > buffer=0x%08lX buffer->data=0x%08lX",
            (unsigned long) buffer,
            (unsigned long) buffer->data);
#endif

        // Initialize buffer values.

        buffer->bufferId    = bufferIdSequential;
        buffer->bank        = bank;
        buffer->prev        = NULL;
        buffer->next        = NULL;
        buffer->bufferSize  = bufferSize;
        buffer->pilotSize   = pilotSize;
        buffer->dataSize    = 0;
#ifdef MMPS_USE_OWNER_ID
        buffer->ownerId     = MMPS_NO_OWNER;
#endif
        buffer->cursor      = buffer->data;
        buffer->dmaAddress  = NULL;

        bufferIdSequential++;
        bufferIdInBlock++;
    }

    pool->banks[bankId] = bank;

#ifdef MMPS_INIT_BANK
    ReportDebug("Created bank %u with %u buffers of size %u, with total bank size %lld MB",
        bankId,
        bank->numberOfBuffers,
        bufferSize,
        totalBlockSize >> 20);

    ReportDebug("There are %u block(s) of size %lu KB each and %lu KB last",
        bank->numberOfBlocks,
        bank->eachBlockSize >> 10,
        bank->lastBlockSize >> 10);
#endif

    return MMPS_OK;
}

#ifdef MMPS_DMA
/**
 * MMPS_DMAMapBufferBank()
 * Map data blocks of all buffer handlers of the specified bank to DMA.
 *
 * @pool:               Pointer to MMPS pool handler.
 * @bankId:             Bank id.
 * @dmaCapabilities:    DMA capabilities bitmask as described by mmap().
 * @dmaFlags:           DMA flags bitmask as described by mmap().
 *
 * Returns 0 upon successful completion or an error code (negative value),
 * if cannot map to DMA.
 */
int
MMPS_DMAMapBufferBank(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    int                 dmaCapabilities,
    int                 dmaFlags)
{
    struct MMPS_Bank    *bank;
    struct MMPS_Buffer  *buffer;
    unsigned int        bufferId;
    int                 rc;

    rc = 0;

    bank = pool->banks[bankId];
    for (bufferId = 0; bufferId < bank->numberOfBuffers; bufferId++)
    {
        buffer = MMPS_BufferById(pool, bankId, bufferId);
        //
        // Function MMPS_BufferById() delivers trusted value - no need for error check.

        rc = MMPS_DMAMapBuffer(buffer, dmaCapabilities, dmaFlags);
        if (rc != 0)
            break;
    }

    return rc;
}

/**
 * MMPS_DMAMapBuffer()
 * Map data block of a single MMPS buffer handler to DMA.
 *
 * @buffer:             Pointer to MMPS buffer handler those data block
 *                      needs to be mapped to DMA.
 * @dmaCapabilities:    DMA capabilities bitmask as described by mmap().
 * @dmaFlags:           DMA flags bitmask as described by mmap().
 *
 * Returns 0 upon successful completion or an error code (negative value),
 * if cannot map to DMA.
 */
int
MMPS_DMAMapBuffer(
    struct MMPS_Buffer  *buffer,
    int                 dmaCapabilities,
    int                 dmaFlags)
{
    void *dmaAddress;

    if (buffer->dmaAddress != NULL)
    {
        ReportWarning("Buffer already mapped to DMA");

        return MMPS_ALREADY_DMA_MAPPED;
    }

    dmaAddress = mmap(buffer->data,
        buffer->bufferSize,
        dmaCapabilities,
        dmaFlags,
        NOFD,
        0);
    if (dmaAddress == MAP_FAILED)
    {
        ReportError("Cannot map buffer to DMA: errno=%d",
            errno);

        return MMPS_CANNOT_MAP_TO_DMA;
    }

    buffer->dmaAddress = dmaAddress;

    return MMPS_OK;
}

/**
 * MMPS_DMAUnmapBuffer()
 * Unmap data blocks of a single MMPS buffer handler from DMA.
 *
 * @buffer: Pointer to MMPS buffer handler those data block
 *          needs to be unmapped from DMA.
 *
 * Returns 0 upon successful completion or an error code (negative value),
 * if cannot unmap from DMA.
 */
int
MMPS_DMAUnmapBuffer(struct MMPS_Buffer *buffer)
{
    int rc;

    if (buffer->dmaAddress == NULL)
    {
        ReportWarning("Trying to unmap buffer from DMA that was not mapped");

        return MMPS_NOT_DMA_MAPPED;
    }

    rc = munmap(buffer->dmaAddress, buffer->bufferSize);
    if (rc != 0)
    {
        ReportError("Cannot unmap buffer from DMA: errno=%d", errno);

        return MMPS_CANNOT_UNMAP_FROM_DMA;
    }

    // Reset DMA address, otherwise this buffer cannot be mapped to DMA again.
    //
    buffer->dmaAddress = NULL;

    return MMPS_OK;
}
#endif

/**
 * MMPS_BufferById()
 * Get buffer for specified MMPS pool and specified buffer bank.
 *
 * @pool:               Pointer to MMPS pool handler.
 * @bankId:             Bank id.
 * @bufferId:           Buffer id.
 *
 * Returns pointer to MMPS buffer handler.
 */
struct MMPS_Buffer *
MMPS_BufferById(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    unsigned int        bufferId)
{
    struct MMPS_Bank    *bank;
    void                *block;
    unsigned int        blockId;
    unsigned int        bufferIdInBlock;
    struct MMPS_Buffer  *buffer;

    bank = pool->banks[bankId];

    blockId = bufferId / bank->buffersPerBlock;
    bufferIdInBlock = bufferId % bank->buffersPerBlock;
    block = bank->blocks[blockId];

    buffer = (void *)
        ((unsigned long) block + (unsigned long) (bufferIdInBlock * sizeof(struct MMPS_Buffer)));

#ifdef MMPS_PEEK_POKE
    ReportDebug("... bufferId=%u bufferIdInBlock=%u block=0x%08lX buffer=0x%08lX data=0x%08lX",
        bufferId,
        bufferIdInBlock,
        (unsigned long) block,
        (unsigned long) buffer,
        (unsigned long) buffer->data);
#endif

    return buffer;
}

/**
 * MMPS_PeekBuffer()
 * Peek buffer from the first bank of the MMPS pool.
 *
 * @pool:               Pointer to MMPS pool handler.
 * @ownerId:            Owner id or application id the buffer will be associated with.
 *                      This value will be stored in a buffer descriptor.
 *
 * Returns pointer to MMPS buffer handler or NULL, if there are no free buffers
 * available in a specified pool.
 */
inline struct MMPS_Buffer *
MMPS_PeekBuffer(
    struct MMPS_Pool    *pool,
    const unsigned int  ownerId)
{
    return MMPS_PeekBufferFromBank(pool, 0, ownerId);
}

/**
 * MMPS_PeekBufferOfSize()
 * Peek a buffer of preferred size from of the specified MMPS pool.
 * MMPS will go through all banks until unused buffer is found.
 * If no buffer of a preferred size can be found, a smaller buffer can be returned.
 *
 * @pool:               Pointer to MMPS pool handler.
 * @preferredSize:      Preferred/minimum size of requested buffer.
 * @ownerId:            Owner id or application id the buffer will be associated with.
 *                      This value will be stored in a buffer descriptor.
 *
 * Returns pointer to MMPS buffer handler or NULL, if there are no free buffers
 * available in a specified pool.
 */
struct MMPS_Buffer *
MMPS_PeekBufferOfSize(
    struct MMPS_Pool    *pool,
    unsigned int        preferredSize,
    const unsigned int  ownerId)
{
    struct MMPS_Bank    *bank;
    struct MMPS_Buffer  *buffer;
    unsigned int        bankId;

#ifdef MMPS_PEEK_POKE
    ReportDebug("Peek buffer of size %u for 0x%08X",
        preferredSize,
        ownerId);
#endif

    buffer = NULL;

    // Go through all banks, assuming banks are ordered by buffer size.
    //
    for (bankId = 0; bankId < pool->numberOfBanks; bankId++)
    {
        bank = pool->banks[bankId];

        // If buffer size of this bank fits to request ...
        //
        if (bank->bufferSize >= preferredSize)
        {
            // ... then try to get a buffer from this bank.
            //
            buffer = MMPS_PeekBufferFromBank(pool, bankId, ownerId);
            if (buffer != NULL)
                break;

            // If there are no free buffers in this bank, then go long with further banks.
        }
    }

    // If no buffer is being found in the banks of preferred size,
    // then try to get at least any buffer.
    //
    if (buffer == NULL)
    {
        // Go through all banks.
        //
        for (bankId = 0; bankId < pool->numberOfBanks; bankId++)
        {
            bank = pool->banks[bankId];

            // Try to get a buffer.
            //
            buffer = MMPS_PeekBufferFromBank(pool, bankId, ownerId);
            if (buffer != NULL)
                break;
        }
    }

    if (buffer == NULL)
    {
        ReportWarning("No free buffer available (requested preferred size %u for 0x%08X)",
            preferredSize,
            ownerId);
    }

    return buffer;
}

/**
 * MMPS_PeekBufferFromBank()
 * Peek an unused buffer from the specified MMPS bank.
 *
 * @pool:               Pointer to MMPS pool handler.
 * @bankId:             Bank id to search for unused buffer.
 * @ownerId:            Owner id or application id the buffer will be associated with.
 *                      This value will be stored in a buffer handler.
 *
 * Returns pointer to MMPS buffer handler or NULL, if there are no free buffers
 * available in a specified pool.
 */
struct MMPS_Buffer *
MMPS_PeekBufferFromBank(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    const unsigned int  ownerId)
{
    struct MMPS_Bank    *bank;
    struct MMPS_Buffer  *buffer;
    unsigned int        bufferId;

#ifdef MMPS_PEEK_POKE
    ReportDebug("Peek buffer from bank %u for 0x%08X",
        bankId,
        ownerId);
#endif

    bank = pool->banks[bankId];

#ifdef MMPS_MUTEX
    pthread_mutex_lock(&bank->mutex);
#else
    pthread_spin_lock(&bank->lock);
#endif

    bufferId = bank->ids[bank->cursor.peek];

#ifdef MMPS_PEEK_POKE
    ReportDebug("... bank->peekCursor=%u bank->pokeCursor=%u bufferId=%u",
        bank->cursor.peek,
        bank->cursor.poke,
        bufferId);
#endif

    if (bufferId == NOTHING) {
        //
        // All buffers are in use (peek cursor has reached the poke cursor
        // and there are no buffers behind the poke cursor in the buffer bank).
        //
        buffer = NULL;
    } else {
        buffer = MMPS_BufferById(pool, bankId, bufferId);

#ifdef MMPS_USE_OWNER_ID
        if (buffer->ownerId != MMPS_NO_OWNER)
        {
#ifdef MMPS_MUTEX
            pthread_mutex_unlock(&bank->mutex);
#else
            pthread_spin_unlock(&bank->lock);
#endif

            ReportError("Got buffer that already belongs to 0x%08X",
                ownerId);

            return NULL;
        }

        buffer->ownerId = ownerId;
#endif

        bank->ids[bank->cursor.peek] = NOTHING;

        bank->cursor.peek++;
        if (bank->cursor.peek == bank->numberOfBuffers)
            bank->cursor.peek = 0;
    }

#ifdef MMPS_MUTEX
    pthread_mutex_unlock(&bank->mutex);
#else
    pthread_spin_unlock(&bank->lock);
#endif

    if (buffer == NULL)
    {
        ReportWarning("No free buffer available (requested bank %u for 0x%08X)",
            bankId,
            ownerId);
    }

    return buffer;
}

/**
 * MMPS_PokeBuffer()
 * Poke buffer or buffer chain back to MMPS pool. In case of chain of buffers,
 * the chain will be disassembled and each buffer will be put back
 * to a corresponding bank.
 *
 * @buffer: Pointer to a buffer handler or a chain of buffer handlers
 *          to be put back to MMPS pool.
 */
void
MMPS_PokeBuffer(struct MMPS_Buffer *buffer)
{
    struct MMPS_Bank    *bank;
    struct MMPS_Buffer  *thisBuffer;
    struct MMPS_Buffer  *nextBuffer;

    thisBuffer = MMPS_FirstBuffer(buffer);
    do {
        bank = thisBuffer->bank;

#ifdef MMPS_PEEK_POKE
#ifdef MMPS_USE_OWNER_ID
        ReportDebug("Poke buffer %u to bank %u for 0x%08X",
            thisBuffer->bufferId, bank->bankId, thisBuffer->ownerId);
#else
        ReportDebug("Poke buffer %u to bank %u",
            thisBuffer->bufferId, bank->bankId);
#endif
#endif

#ifdef MMPS_USE_OWNER_ID
        if (thisBuffer->ownerId == MMPS_NO_OWNER)
        {
            ReportError("Trying to poke buffer %u to bank %u that was not peeked",
                thisBuffer->bufferId,
                thisBuffer->bank->bankId);

            break;
        }
#endif

        nextBuffer = thisBuffer->next;

        if ((nextBuffer != NULL) && (nextBuffer->prev != thisBuffer))
        {
            ReportError("Detected broken buffer chain for buffers %u and %u",
                thisBuffer->bufferId,
                nextBuffer->bufferId);

            nextBuffer = NULL;
        }

        thisBuffer->prev = NULL;
        thisBuffer->next = NULL;
        thisBuffer->dataSize = 0;
        thisBuffer->cursor = thisBuffer->data;
#ifdef MMPS_USE_OWNER_ID
        thisBuffer->ownerId = MMPS_NO_OWNER;
#endif

#ifdef MMPS_MUTEX
        pthread_mutex_lock(&bank->mutex);
#else
        pthread_spin_lock(&bank->lock);
#endif

        bank->ids[bank->cursor.poke] = thisBuffer->bufferId;

        bank->cursor.poke++;
        if (bank->cursor.poke == bank->numberOfBuffers)
            bank->cursor.poke = 0;

#ifdef MMPS_MUTEX
        pthread_mutex_unlock(&bank->mutex);
#else
        pthread_spin_unlock(&bank->lock);
#endif

        thisBuffer = nextBuffer;

#ifdef MMPS_PEEK_POKE
        if (thisBuffer != NULL)
        {
            ReportDebug("... next buffer %u",
                thisBuffer->bufferId);
        }
#endif
    } while (thisBuffer != NULL);
}

/**
 * MMPS_PreviousBuffer()
 * Get buffer from a chain of buffers which precedes the referencing buffer.
 *
 * @buffer: Pointer to the referencing MMPS buffer handler.
 *
 * Returns pointer to MMPS buffer handler of the preceding buffer in a chain
 * or NULL, if referencing buffer is the first buffer in a chain.
 */
inline struct MMPS_Buffer *
MMPS_PreviousBuffer(struct MMPS_Buffer *buffer)
{
    return buffer->prev;
}

/**
 * MMPS_NextBuffer()
 * Get buffer from a chain of buffers which succeeds the referencing buffer.
 *
 * @buffer: Pointer to the referencing MMPS buffer handler.
 *
 * Returns pointer to MMPS buffer handler of the succeeding buffer in a chain
 * or NULL, if referencing buffer is the last buffer in a chain.
 */
inline struct MMPS_Buffer *
MMPS_NextBuffer(struct MMPS_Buffer *buffer)
{
    return buffer->next;
}

/**
 * MMPS_FirstBuffer()
 * Get first buffer of a chain.
 *
 * @chain: Pointer to the referencing MMPS buffer handler.
 *
 * Returns pointer to MMPS buffer handler of the first buffer in a chain.
 */
inline struct MMPS_Buffer *
MMPS_FirstBuffer(struct MMPS_Buffer *chain)
{
    struct MMPS_Buffer *firstBuffer;

    firstBuffer = chain;

    while (firstBuffer->prev != NULL)
        firstBuffer = firstBuffer->prev;

    return firstBuffer;
}

/**
 * MMPS_LastBuffer()
 * Get last buffer of a chain.
 *
 * @chain: Pointer to the referencing MMPS buffer handler.
 *
 * Returns pointer to MMPS buffer handler of the last buffer in a chain.
 */
inline struct MMPS_Buffer *
MMPS_LastBuffer(struct MMPS_Buffer *chain)
{
    struct MMPS_Buffer *lastBuffer;

    lastBuffer = chain;

    while (lastBuffer->next != NULL)
        lastBuffer = lastBuffer->next;

    return lastBuffer;
}

/**
 * MMPS_ExtendBuffer()
 * Peek one more buffer from the MMPS pool and append it at the end
 * of a specified chain of buffers. MMPS will try to peek a buffer
 * from the same bank, the referencing buffer belongs to. If there no buffers
 * available in that bank, MMPS will search through all other banks for a free buffer.
 *
 * @buffer: Buffer to append a new buffer to.
 *
 * Returns pointer to MMPS buffer handler of the attached buffer or NULL,
 * if MMPS was not able to find any free buffer in a pool.
 * If referenced buffer did already have a succeeding buffer (chain),
 * then the pointer to MMPS buffer handler of the succeeding buffer will be returned
 */
struct MMPS_Buffer *
MMPS_ExtendBuffer(struct MMPS_Buffer *origBuffer)
{
    struct MMPS_Buffer *nextBuffer;

    nextBuffer = MMPS_PeekBufferOfSize(
        origBuffer->bank->pool,
        origBuffer->bufferSize,
#ifdef MMPS_USE_OWNER_ID
        origBuffer->ownerId
#else
        MMPS_NO_OWNER
#endif
    );

    origBuffer->next = nextBuffer;
    nextBuffer->prev = origBuffer;

    return nextBuffer;
}

/**
 * MMPS_AppendBuffer()
 * Append buffer or a chain of buffers to another buffer or another chain of buffers.
 *
 * @destination:        Pointer to MMPS buffer handler from the chain
 *                      to append new buffer to.
 * @appendage:          Pointer to MMPS buffer handler that has to be appended
 *                      at the end of a chain.
 *
 * If a valid pointer to MMPS buffer handler is provided as destination reference,
 * then the same pointer is returned.
 * If destination is referenced as NULL, then the appendage reference is returned.
 */
struct MMPS_Buffer *
MMPS_AppendBuffer(
    struct MMPS_Buffer *destination,
    struct MMPS_Buffer *appendage)
{
    struct MMPS_Buffer *buffer;

    if (destination == NULL)
    {
        return appendage;
    }
    else
    {
        buffer = MMPS_LastBuffer(destination);
        buffer->next = appendage;
        appendage->prev = buffer;
        return destination;
    }
}

/**
 * MMPS_TruncateBuffer()
 * Cut buffers chained on a referenced buffer and give the cut buffers back to the pool.
 * All buffers, succeeding the referenced buffer, will be removed from the chain
 * and put back to the MMPS pool.
 * If referenced buffer is the first buffer in a chain, then after completion
 * of MMPS_TruncateBuffer() this buffer will be a standalone buffer (not chained buffer).
 * If referenced buffer is not the first buffer in a chain, then after completion
 * of MMPS_TruncateBuffer() this buffer will be the last buffer of a chain..
 *
 * @buffer: Pointer to MMPS buffer handler of a referenced buffer.
 *
 * Does not return any value.
 */
void
MMPS_TruncateBuffer(struct MMPS_Buffer *buffer)
{
    if (buffer->next != NULL)
    {
        MMPS_PokeBuffer(buffer->next);
        buffer->next = NULL;
    }
}

/**
 * MMPS_CopyBuffer()
 * Copy data from one buffer chain to another. If destination chain of buffers
 * is smaller the source then only a part of data will be copied.
 *
 * @destination:        Pointer to MMPS buffer handler of the destination buffer chain.
 * @source:             Pointer to MMPS buffer handler of the source buffer chain.
 *
 * Returns number of bytes being copied.
 */
unsigned int
MMPS_CopyBuffer(struct MMPS_Buffer *destination, struct MMPS_Buffer *source)
{
    char            *destinationCursor;
    char            *sourceCursor;
    unsigned int    destinationBytes;
    unsigned int    sourceBytes;
    unsigned int    bytesToCopy;
    unsigned int    totalCopiedBytes;

    totalCopiedBytes = 0;

    destinationCursor = destination->data;
    sourceCursor = source->data;

    destinationBytes = destination->bufferSize;
    sourceBytes = source->dataSize;

    while (destination != NULL)
    {
        // If current destination buffer is full, then switch
        // to the next buffer in a destination chain. If there are no other buffers
        // in a destination chain then the job is done
        //
        if (destinationBytes == 0)
        {
            destination = MMPS_NextBuffer(destination);
            if (destination == NULL)
                break;

            destinationCursor = destination->data;
            destinationBytes = destination->bufferSize;
        }

        // If current source buffer is copied completely, then switch
        // to the next buffer in a source chain. If there are no other buffers
        // in a source chain then the job is done
        //
        if (sourceBytes == 0)
        {
            source = MMPS_NextBuffer(source);
            if (source == NULL)
                break;

            sourceCursor = source->data;
            sourceBytes = source->dataSize;
        }

        if (destinationBytes < sourceBytes) {
            //
            // Either copy as much as may fit in a current destination buffer ...
            //
            bytesToCopy = destinationBytes;
        } else {
            //
            // ... or as much as is left in a current source buffer.
            //
            bytesToCopy = sourceBytes;
        }

        // Copy piece of data.
        //
        memcpy(destinationCursor, sourceCursor, bytesToCopy);

        totalCopiedBytes += bytesToCopy;

        // Move both cursors forwards.
        //
        destinationCursor += bytesToCopy;
        sourceCursor += bytesToCopy;

        // And decrease both bytes counters.
        //
        destinationBytes -= bytesToCopy;
        sourceBytes -= bytesToCopy;
    }

    return totalCopiedBytes;
}

/**
 * MMPS_TotalDataSize()
 * Get total data size summarized for all buffers in a chain.
 * The buffers in a chain may belong to different banks and be of different size.
 *
 * @firstBuffer: Pointer to MMPS buffer handler of the first buffer in a chain
 *               to begin counting from.
 *
 * Returns total data size in bytes.
 */
inline unsigned int
MMPS_TotalDataSize(struct MMPS_Buffer *firstBuffer)
{
    unsigned int        totalDataSize;
    struct MMPS_Buffer  *buffer;

    totalDataSize = 0;
    for (buffer = firstBuffer; buffer != NULL; buffer = buffer->next)
        totalDataSize += buffer->dataSize;

    return totalDataSize;
}

/**
 * MMPS_ResetBufferData()
 * Reset data size to 0 for referencing buffer or, if a chain of buffers is referenced,
 * for all buffers in a chain.
 *
 * @buffer:             Pointer to MMPS buffer handler or anchor of a chain of buffers.
 * @leavePilot:         Boolean value specifying whether pilot has to be left
 *                      in the anchor buffer.
 */
inline void
MMPS_ResetBufferData(struct MMPS_Buffer *buffer, unsigned int leavePilot)
{
    // Reset data size of the first buffer in a chain. Leave pilot if needed.
    //
    if (leavePilot == 0) {
        buffer->dataSize = 0;
    } else {
        buffer->dataSize = buffer->pilotSize;
    }

    // Reset data size of the rest buffers in a chain.
    //
    buffer = buffer->next;
    while (buffer != NULL)
    {
        buffer->dataSize = 0;
        buffer = buffer->next;
    }
}

/**
 * MMPS_ResetCursor()
 * Reset cursor to position to the beginning of data for referencing buffer or,
 * if a chain of buffers is referenced, for all buffers in a chain.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @leavePilot:         Boolean value specifying whether cursor must point
 *                      to a beginning of data or to the beginning of payload
 *                      after the pilot.
 */
inline void
MMPS_ResetCursor(struct MMPS_Buffer *buffer, unsigned int leavePilot)
{
    // Move cursor of the first buffer in a chain to wither the beginning of data
    // or beginning of payload after the pilot.
    //
    if (leavePilot == 0) {
        buffer->cursor = buffer->data;
    } else {
        buffer->cursor = buffer->data + buffer->pilotSize;
    }

    // The rest of buffers in a chain do not contain pilot.
    // Just move cursor to the beginning.
    //
    buffer = buffer->next;
    while (buffer != NULL)
    {
        buffer->cursor = buffer->data;
        buffer = buffer->next;
    }
}

/**
 * MMPS_MoveCursorRelative()
 * Move cursor forwards for specified amount of bytes. If new absolute offset
 * is located in another buffer of a chain then switch from the referencing buffer
 * to succeeding buffers through the chain until a proper buffer is reached.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @relativeOffset:     Number of bytes to move cursor forwards.
 *
 * Returns pointer to MMPS buffer handler of a buffer where the cursor is positioned
 * after the move operation. If new the absolute position, calculated by current
 * cursor position and relative offset, is out of range of the referencing buffer
 * or chain of buffers, then a NULL pointer is returned
 */
struct MMPS_Buffer *
MMPS_MoveCursorRelative(
    struct MMPS_Buffer  *buffer,
    unsigned int        relativeOffset)
{
    unsigned int        curTotalDataSize;
    unsigned int        newTotalDataSize;
    int                 deltaForExtensionBufferTotal;
    int                 deltaForExtensionBufferStep;
    struct MMPS_Buffer  *currentBuffer;

    // Get total data size for a complete chain.
    //
    curTotalDataSize = MMPS_TotalDataSize(MMPS_FirstBuffer(buffer));

    // Get new total data size (inklusive new data mentioned by relative offset).
    //
    newTotalDataSize = curTotalDataSize + relativeOffset;

    // Find out how many bytes would (possibly) not fit in the current buffer.
    //
    deltaForExtensionBufferTotal =
        buffer->dataSize - (buffer->cursor - buffer->data) - relativeOffset;

    // Delta equal 0 means that the new data will fit up to the end of the current buffer.
    // Delta greater than 0 means that the new data will fit in the current buffer
    // and there will be still free space left.
    // Negative value shows how many bytes would not fit in the current buffer.
    //
    if (deltaForExtensionBufferTotal >= 0)
    {
        buffer->dataSize = newTotalDataSize;

        buffer->cursor += relativeOffset;

        return buffer;
    }
    else
    {
        // Revert delat so that it presents a positive number of bytes.
        //
        deltaForExtensionBufferTotal = -deltaForExtensionBufferTotal;

        currentBuffer = buffer;
        while (deltaForExtensionBufferTotal > 0)
        {
            deltaForExtensionBufferStep = currentBuffer->bufferSize - currentBuffer->dataSize;
            if (deltaForExtensionBufferStep > relativeOffset)
                deltaForExtensionBufferStep = relativeOffset;

            currentBuffer->dataSize += deltaForExtensionBufferStep;

            currentBuffer->cursor += deltaForExtensionBufferStep;

            deltaForExtensionBufferTotal -= deltaForExtensionBufferStep;

            // Extend buffer (or chain) if needed.
            //
            if (deltaForExtensionBufferTotal > 0)
            {
                currentBuffer = MMPS_ExtendBuffer(currentBuffer);
                if (currentBuffer == NULL)
                    return NULL;
            }
        }

        return currentBuffer;
    }
}

/**
 * MMPS_PutData()
 * Put (write) data to a buffer chain under cursor. Append new buffer if necessary.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @sourceData:         Pointer to memory area to copy data from.
 * @sourceDataSize:     Size of data to copy.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after write is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutData(
    struct MMPS_Buffer  *buffer,
    const char* const   sourceData,
    unsigned int        sourceDataSize)
{
    unsigned int sourceDataOffset;

    sourceDataOffset = 0;
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
        memcpy(
            buffer->data + buffer->dataSize,
            sourceData + sourceDataOffset,
            bytesPerBuffer);

        buffer->dataSize += bytesPerBuffer;

        sourceDataOffset += bytesPerBuffer;

        // If free space in current buffer was not enough to write complete data ...
        //
        if (sourceDataOffset < sourceDataSize)
        {
            // ... then switch to next buffer in a chain or append a new buffer if necessary.
            //
            if (buffer->next != NULL) {
                //
                // Switch to next buffer.
                //
                buffer = buffer->next;
            } else {
                buffer = MMPS_ExtendBuffer(buffer);
                if (buffer == NULL)
                    return NULL;
            }
        }
    }

    return buffer;
}

/**
 * MMPS_GetData()
 * Get (read) data from a buffer chain under cursor.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @destData:           Pointer to memory area to copy data to.
 * @destDataSize:       Size of data to copy.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after read is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetData(
    struct MMPS_Buffer  *buffer,
    char* const         destData,
    unsigned int        destDataSize)
{
    unsigned int destDataOffset;

    destDataOffset = 0;
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
        if (destDataOffset < destDataSize)
        {
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
 * MMPS_PutInt8()
 * Put one byte to buffer chain under cursor.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @sourceData:         Pointer to memory area to copy data from.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after write is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutInt8(
    struct MMPS_Buffer  *buffer,
    uint8               *sourceData)
{
    buffer = MMPS_PutData(buffer, (char *) sourceData, sizeof(uint8));
    return buffer;
}

/**
 * MMPS_GetInt8()
 * Get one byte from buffer chain under cursor.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @destData:           Pointer to memory area to copy data to.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after read is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetInt8(
    struct MMPS_Buffer  *buffer,
    uint8               *destData)
{
    buffer = MMPS_GetData(buffer, (char *) destData, sizeof(uint8));
    return buffer;
}

/**
 * MMPS_PutInt16()
 * Put one word (two bytes) to buffer chain under cursor.
 * Do endian conversion if needed.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @sourceData: Pointer to data.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after write is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutInt16(
    struct MMPS_Buffer  *buffer,
    uint16              *sourceData)
{
    uint16 value = htobe16(*sourceData);
    buffer = MMPS_PutData(buffer, (char *) &value, sizeof(value));
    return buffer;
}

/**
 * MMPS_GetInt16()
 * Get one word (two bytes) from buffer chain under cursor.
 * Do endian conversion if needed.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @destData:           Pointer to memory area to copy data to.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after read is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetInt16(
    struct MMPS_Buffer  *buffer,
    uint16              *destData)
{
    uint16 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value));
    *destData = be16toh(value);
    return buffer;
}

/**
 * MMPS_PutInt32()
 * Put one double word (four bytes) to buffer chain under cursor.
 * Do endian conversion if needed
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @sourceData:         Pointer to memory area to copy data from.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after write is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutInt32(
    struct MMPS_Buffer  *buffer,
    uint32              *sourceData)
{
    uint32 value = htobe32(*sourceData);
    buffer = MMPS_PutData(buffer, (char *) &value, sizeof(value));
    return buffer;
}

/**
 * MMPS_GetInt32()
 * Get one double word (four bytes) from buffer chain under cursor.
 * Do endian conversion if needed.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @destData:           Pointer to memory area to copy data to.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after read is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetInt32(
    struct MMPS_Buffer  *buffer,
    uint32              *destData)
{
    uint32 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value));
    *destData = be32toh(value);
    return buffer;
}

/**
 * MMPS_PutInt64()
 * Put one quad word (eight bytes) to buffer chain under cursor.
 * Do endian conversion if needed.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @sourceData:         Pointer to memory area to copy data from.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after write is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutInt64(
    struct MMPS_Buffer  *buffer,
    uint64              *sourceData)
{
    uint64 value = htobe64(*sourceData);
    buffer = MMPS_PutData(buffer, (char *) &value, sizeof(value));
    return buffer;
}

/**
 * MMPS_GetInt64()
 * Get one quad word (eight bytes) from buffer chain under cursor.
 * Do endian conversion if needed.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @destData:           Pointer to memory area to copy data to.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after read is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetInt64(
    struct MMPS_Buffer  *buffer,
    uint64              *destData)
{
    uint64 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value));
    *destData = be64toh(value);
    return buffer;
}

/**
 * MMPS_PutFloat32()
 * Put one 32-bit floating-point value (four bytes) to buffer chain under cursor.
 * Do endian conversion if needed
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @sourceData:         Pointer to memory area to copy data from.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after write is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutFloat32(
    struct MMPS_Buffer  *buffer,
    float               *sourceData)
{
    float32 value = (float32) htobe32(*sourceData);
    buffer = MMPS_PutData(buffer, (char *) &value, sizeof(value));
    return buffer;
}

/**
 * MMPS_GetFloat32()
 * Get one 32-bit floating-point value (four bytes) from buffer chain under cursor.
 * Do endian conversion if needed.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @destData:           Pointer to memory area to copy data to.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after read is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetFloat32(
    struct MMPS_Buffer  *buffer,
    float               *destData)
{
    float32 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value));
    value = (float32) be64toh((uint32) value);
    *destData = value;
    return buffer;
}

/**
 * MMPS_PutFloat64()
 * Put one 64-bit double precision floating-point value (eight bytes)
 * to buffer chain under cursor. Do endian conversion if needed.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @sourceData:         Pointer to memory area to copy data from.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after write is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutFloat64(
    struct MMPS_Buffer  *buffer,
    double              *sourceData)
{
    float64 value = (float64) htobe64(*sourceData);
    buffer = MMPS_PutData(buffer, (char *) &value, sizeof(value));
    return buffer;
}

/**
 * MMPS_GetFloat64()
 * Get one 64-bit double precision floating-point value (eight bytes)
 * from buffer chain under cursor. Do endian conversion if needed.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @destData:           Pointer to memory area to copy data to.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after read is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetFloat64(
    struct MMPS_Buffer  *buffer,
    double              *destData)
{
    float64 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value));
    value = (float64) be64toh((uint64) value);
    *destData = value;
    return buffer;
}

/**
 * MMPS_PutString()
 *
 * @buffer:             Pointer to MMPS buffer handler.
 * @sourceData:         Pointer to memory area to copy data from.
 * @destDataSize:       Size of data to copy.
 *
 * Returns pointer to MMPS handler of a buffer in which a cursor is pointing to,
 * after write is complete. This may be another buffer than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutString(
    struct MMPS_Buffer  *buffer,
    char                *sourceData,
    unsigned int        sourceDataSize)
{
    uint32 swapped = htobe32(sourceDataSize);
    buffer = MMPS_PutData(buffer, (char *)&swapped, sizeof(swapped));
    if (sourceDataSize != 0)
        buffer = MMPS_PutData(buffer, sourceData, sourceDataSize);
    return buffer;
}

/**
 * MMPS_NumberOfBuffersInUse()
 * Get number of used MMPS buffers for the specified buffer bank. This function
 * may be used for debugging purposes to check periodically how many MMPS buffers
 * are in use (peeked out of the bank). Do not use this function in a production code
 * as it may cause performance degradation.
 *
 * @pool:               Pointer to MMPS pool handler to get statistical information for.
 * @bankId:             Buffer bank id.
 *
 * Returns the number of buffers 'in use'.
 */
unsigned int
MMPS_NumberOfBuffersInUse(
    struct MMPS_Pool    *pool,
    unsigned int        bankId)
{
    struct MMPS_Bank    *bank;
    unsigned int        buffersInUse;

    bank = pool->banks[bankId];

#ifdef MMPS_MUTEX
    pthread_mutex_lock(&bank->mutex);
#else
    pthread_spin_lock(&bank->lock);
#endif

    buffersInUse = (bank->cursor.peek >= bank->cursor.poke)
        ? bank->cursor.peek - bank->cursor.poke
        : bank->numberOfBuffers - (bank->cursor.poke - bank->cursor.peek);

#ifdef MMPS_MUTEX
    pthread_mutex_unlock(&bank->mutex);
#else
    pthread_spin_unlock(&bank->lock);
#endif

    return buffersInUse;
}
