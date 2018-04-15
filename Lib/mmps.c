// System definition files.
//
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#ifndef LINUX
#include <atomic.h>
#endif

// Common definition files.
//
#include "report.h"
#include "Types.h"

// Local definition files.
//
#include "mmps.h"

const unsigned int NOTHING = 0xFFFF0000;

/*
 * @brief   Allocate and initialize buffer pool.
 *
 * @param   numberOfBanks   Number of banks to allocate.
 *                          Each bank has to be initialized
 *                          afterwards using MMPS_InitBank().
 *
 * @return  Pointer to MMPS pool descriptor upon successful completion.
 * @return  NULL if MMPS pool cannot be created.
 */
struct MMPS_Pool *
MMPS_InitPool(const unsigned int numberOfBanks)
{
    struct MMPS_Pool    *pool;
    unsigned int        size;

    size = sizeof(struct MMPS_Pool) + numberOfBanks * sizeof(struct MMPS_Bank *);
    pool = malloc(size);
    if (pool == NULL)
    {
        ReportSoftAlert("[MMPS] Out of memory");

        return NULL;
    }

#ifdef MMPS_EYECATCHER
    strncpy(pool->eyeCatcher, EYECATCHER_POOL, EYECATCHER_SIZE);
#endif

    pool->numberOfBanks = numberOfBanks;

    // Make sure pointers to buffer banks are set to null.
    //
    memset(&pool->banks, 0, numberOfBanks * sizeof(struct MMPS_Bank *));

    return pool;
}

/*
 * @brief   Initialize buffer bank - MMPS pool must be already initialized.
 *
 * This routine allocates and initializes memory structures
 * necessary for bank descriptor and all buffers of the specified bank.
 *
 * @param   pool        Pointer to MMPS pool descriptor - already initialized
 *                      with MMPS_InitPool().
 * @param   bankId      Bank id of the bank to be initialized.
 * @param   bufferSize  Value greater than 0 specifies the size of buffers
 *                      in a bank. All buffers in a bank will be created
 *                      of the given size. Value of 0 specifies
 *                      that the specified bank will contain buffers
 *                      of different sizes. In such case MMPS will only
 *                      allocate buffer descriptor but not data blocks.
 *                      Data blocks could be allocated for buffer descriptors
 *                      later.
 * @param   followerSize    Size of the follower or 0 if buffers in this bank
 *                          are not supposed to have followers.
 * @param   numberOfBuffers Number of buffer descriptor to be created
 *                          in the specified bank. Different banks in one
 *                          MMPS poolmay have different number of buffers.
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), if bank cannot be created.
 */
int
MMPS_InitBank(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    unsigned int        bufferSize,
    unsigned int        followerSize,
    unsigned int        numberOfBuffers)
{
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
    ReportDebug("[MMPS] Create bank %u with %u buffers of size %u",
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
    ReportDebug("[MMPS] " \
            "... maxBlockSize=%lu buffersPerBlock=%u numberOfBlocks=%u " \
            "eachBlockSize=%lu lastBlockSize=%lu",
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
        ReportSoftAlert("[MMPS] Out of memory");

        return MMPS_OUT_OF_MEMORY;
    }

#ifdef MMPS_EYECATCHER
    strncpy(bank->eyeCatcher, EYECATCHER_BANK, EYECATCHER_SIZE);
#endif

    // Initialize bank values.

#ifdef MMPS_MUTEX
    pthread_mutexattr_init(&bank->mutex.attr);

    pthread_mutexattr_setpshared(&bank->mutex.attr, PTHREAD_PROCESS_PRIVATE);

    pthread_mutex_init(&bank->mutex.lock, &bank->mutex.attr);
#else
    pthread_spin_init(&bank->lock, PTHREAD_PROCESS_PRIVATE);
#endif

    bank->bankId              = bankId;
    bank->allocateOnDemand    = FALSE;
    bank->pool                = pool;
    bank->sharedMemoryHandle  = 0;
    bank->numberOfBuffers     = numberOfBuffers;
    bank->bufferSize          = bufferSize;
    bank->followerSize        = followerSize;
    bank->numberOfBlocks      = numberOfBlocks;
    bank->eachBlockSize       = eachBlockSize;
    bank->lastBlockSize       = lastBlockSize;
    bank->buffersPerBlock     = buffersPerBlock;
    bank->cursor.peek         = 0;
    bank->cursor.poke         = 0;

    bank->blocks = (void *) (
        (unsigned long) bank +
        (unsigned long) (bankAllocSize - numberOfBlocks * sizeof(void *))
    );

#ifdef MMPS_INIT_BANK
    ReportDebug("[MMPS] ... bank=0x%016lX (%lu bytes) bank->blocks=0x%016lX",
            (unsigned long) bank,
            bankAllocSize,
            (unsigned long) bank->blocks);
#endif

    blockId = 0;
    bufferIdSequential = 0;
    bufferIdInBlock = buffersPerBlock;

    // Set block to null to omit compiler messages like 'block may be used
    // uninitialized'.
    //
    block = NULL;

    // Allocate all parts of buffer queue, allocate all buffers
    // and put them in a queue.
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
                ReportSoftAlert("[MMPS] Out of memory");

                return MMPS_OUT_OF_MEMORY;
            }

#ifdef MMPS_INIT_BANK_DEEP
            ReportDebug("[MMPS]   > allocated block=0x%016lX",
                    (unsigned long) block);
#endif

            bank->blocks[blockId] = block;
            blockId++;
        }

        bank->ids[bufferIdSequential] = bufferIdSequential;

        buffer = (void *)
            ((unsigned long) block +
            (unsigned long) (bufferIdInBlock * sizeof(struct MMPS_Buffer)));

#ifdef MMPS_EYECATCHER
        strncpy(buffer->eyeCatcher, EYECATCHER_BUFFER, EYECATCHER_SIZE);
#endif

#ifdef MMPS_INIT_BANK_DEEP
        ReportDebug("[MMPS]     > buffer=0x%016lX bufferId=%u",
                (unsigned long) buffer,
                bufferIdSequential);
#endif

        // Initialize buffer values.

        buffer->bufferId      = bufferIdSequential;
        buffer->bank          = bank;
        buffer->prev          = NULL;
        buffer->next          = NULL;
        buffer->ownerId       = MMPS_NO_OWNER;
        buffer->touches       = 0;
        buffer->bufferSize    = bufferSize;
        buffer->followerSize  = followerSize;
        buffer->dataSize      = 0;
        buffer->data          = NULL;
        buffer->cursor        = NULL;
        buffer->dmaAddress    = NULL;

        bufferIdSequential++;
        bufferIdInBlock++;
    }

    pool->banks[bankId] = bank;

#ifdef MMPS_INIT_BANK
    ReportDebug("[MMPS] Created bank %u with %u buffers of size %u, " \
            "with total bank size %lld MB",
            bankId,
            bank->numberOfBuffers,
            bufferSize,
            totalBlockSize >> 20);

    ReportDebug("[MMPS] There are %u block(s) of size %lu KB each and %lu KB last",
            bank->numberOfBlocks,
            bank->eachBlockSize >> 10,
            bank->lastBlockSize >> 10);
#endif

    return MMPS_OK;
}

/*
 * @brief   Allocate data memory blocks for all buffers of the specified bank.
 *
 * @param   pool        Pointer to MMPS pool descriptor - already initialized
 *                      with MMPS_InitPool().
 * @param   bankId      Bank id of the bank those buffers to allocate.
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), in case of error.
 */
int
MMPS_AllocateImmediately(
    struct MMPS_Pool    *pool,
    unsigned int        bankId)
{
    struct MMPS_Bank    *bank;
    struct MMPS_Buffer  *buffer;
    unsigned int        bufferId;

    bank = pool->banks[bankId];

    for (bufferId = 0; bufferId < bank->numberOfBuffers; bufferId++)
    {
        buffer = MMPS_BufferById(pool, bankId, bufferId);
        //
        // Function MMPS_BufferById() delivers trusted value,
        // therefore, no need for error check.

        buffer->data = malloc(buffer->bufferSize);
        if (buffer->data == NULL)
        {
            ReportSoftAlert("[MMPS] Out of memory");

            return MMPS_OUT_OF_MEMORY;
        }

        MMPS_ResetCursor(buffer);
    }

    return MMPS_OK;
}

/*
 * @brief   Allocate follower memory blocks for all buffers of the specified bank.
 *
 * @param   pool        Pointer to MMPS pool descriptor - already initialized
 *                      with MMPS_InitPool().
 * @param   bankId      Bank id of the bank those followers to allocate.
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), in case of error.
 */
extern int
MMPS_AllocateFollowers(
    struct MMPS_Pool    *pool,
    unsigned int        bankId)
{
    struct MMPS_Bank    *bank;
    struct MMPS_Buffer  *buffer;
    unsigned int        bufferId;

    bank = pool->banks[bankId];

    for (bufferId = 0; bufferId < bank->numberOfBuffers; bufferId++)
    {
        buffer = MMPS_BufferById(pool, bankId, bufferId);
        //
        // Function MMPS_BufferById() delivers trusted value,
        // therefore, no need for error check.

        buffer->follower = malloc(buffer->followerSize);
        if (buffer->follower == NULL)
        {
            ReportSoftAlert("[MMPS] Out of memory");

            return MMPS_OUT_OF_MEMORY;
        }
    }

    return MMPS_OK;
}

/**
 * @brief   Set the 'allocate on demand' flag for the specified bank.
 *
 * @param   pool        Pointer to MMPS pool descriptor - already initialized
 *                      with MMPS_InitPool().
 * @param   bankId      Bank id of the bank set as 'allocate on demand'.
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), in case of error.
 */
extern int
MMPS_AllocateOnDemand(
    struct MMPS_Pool    *pool,
    unsigned int        bankId)
{
    struct MMPS_Bank *bank;

    bank = pool->banks[bankId];

    bank->allocateOnDemand = TRUE;

    return 0;
}

/*
 * @brief   Map data blocks to shared memory.
 *
 * Map data blocks of all buffer descriptor of the specified bank
 * to shared memory.
 *
 * @param   pool        Pointer to MMPS pool descriptor.
 * @param   bankId      Bank id.
 * @param   sharedMemoryName File name to be used for shared memory.
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), if cannot map to shared memory.
 */
int
MMPS_MapShMemBufferBank(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    const char          *sharedMemoryName)
{
    struct MMPS_Bank    *bank;
    struct MMPS_Buffer  *buffer;
    int                 sharedMemoryHandle;
    unsigned int        sharedMemorySize;
    unsigned int        bufferId;
    int                 rc;

    rc = MMPS_OK;

    bank = pool->banks[bankId];

    sharedMemoryHandle = shm_open(
            sharedMemoryName,
            O_CREAT | O_RDWR,
            0777);
    if (sharedMemoryHandle == -1)
    {
        ReportError("[MMPS] Cannot open shared memory: errno=%d", errno);

        return MMPS_SHM_ERROR;
    }

    sharedMemorySize = bank->numberOfBuffers * bank->bufferSize;

    rc = ftruncate(sharedMemoryHandle, sharedMemorySize);
    if (rc == -1)
    {
        ReportError("[MMPS] Cannot set shared memory size to %u: errno=%d",
                sharedMemorySize,
                errno);

        return MMPS_SHM_ERROR;
    }

    bank->sharedMemoryHandle = sharedMemoryHandle;

#ifndef MMPS_USE_64BIT_MMAP
    bank->sharedMemoryAddress = mmap(
            NULL,
            sharedMemorySize,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            sharedMemoryHandle,
            0);
#else
    bank->sharedMemoryAddress = mmap64(
            NULL,
            sharedMemorySize,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            sharedMemoryHandle,
            0);
#endif

    if (bank->sharedMemoryAddress == MAP_FAILED)
    {
        ReportError("[MMPS] Cannot map buffer to shared memory: errno=%d",
                errno);

        return MMPS_CANNOT_MAP_TO_SHM;
    }

    for (bufferId = 0; bufferId < bank->numberOfBuffers; bufferId++)
    {
        buffer = MMPS_BufferById(pool, bankId, bufferId);
        //
        // Function MMPS_BufferById() delivers trusted value,
        // therefore, no need for error check.

        rc = MMPS_MapShMemBuffer(buffer);
        if (rc != 0)
            break;
    }

    return rc;
}

/*
 * @brief   Map single data block to shared memory.
 *
 * Map data block of a single MMPS buffer descriptor to shared memory.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor those data block
 *                      needs to be mapped to shared memory.
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), if cannot map to shared memory.
 */
int
MMPS_MapShMemBuffer(struct MMPS_Buffer *buffer)
{
    struct MMPS_Bank    *bank;
    unsigned long       offset;

    bank = buffer->bank;

    if (buffer->data != NULL)
    {
        ReportWarning("[MMPS] Buffer already mapped to shared memory");

        return MMPS_ALREADY_MAPPED_TO_SHM;
    }

    offset = buffer->bufferId * buffer->bufferSize;

    buffer->data = bank->sharedMemoryAddress + offset;
    buffer->cursor = buffer->data;

    return MMPS_OK;
}

/*
 * @brief   Release single data block from shared memory.
 *
 * Release data block of a single MMPS buffer descriptor from shared memory.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor those data block
 *                      needs to be unmapped from shared memory.
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), if cannot release shared memory mapping.
 */
int
MMPS_UnmapShMemBuffer(struct MMPS_Buffer *buffer)
{
    int rc;

    if (buffer->data == NULL)
    {
        ReportWarning("[MMPS] " \
                "Trying to unmap buffer from shared memory that was not mapped");

        return MMPS_NOT_MAPPED_TO_SHM;
    }

    rc = munmap(buffer->data, buffer->bufferSize);
    if (rc != 0)
    {
        ReportError("[MMPS] Cannot unmap buffer from shared memory: errno=%d",
                errno);

        return MMPS_CANNOT_UNMAP_FROM_SHM;
    }

    return MMPS_OK;
}

#ifdef MMPS_DMA

/*
 * @brief   Map data blocks to DMA.
 *
 * Map data blocks of all buffer descriptor of the specified bank to DMA.
 *
 * @param   pool        Pointer to MMPS pool descriptor.
 * @param   bankId      Bank id.
 * @param   dmaCapabilities DMA capabilities bitmask as described by mmap().
 * @param   dmaFlags    DMA flags bitmask as described by mmap().
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), if cannot map to DMA.
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

    rc = MMPS_OK;

    bank = pool->banks[bankId];
    for (bufferId = 0; bufferId < bank->numberOfBuffers; bufferId++)
    {
        buffer = MMPS_BufferById(pool, bankId, bufferId);
        //
        // Function MMPS_BufferById() delivers trusted value,
        // therefore, no need for error check.

        rc = MMPS_DMAMapBuffer(buffer, dmaCapabilities, dmaFlags);
        if (rc != 0)
            break;
    }

    return rc;
}

/*
 * @brief   Map single data block to DMA.
 *
 * Map data block of a single MMPS buffer descriptor to DMA.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor those data block
 *                      needs to be mapped to DMA.
 * @param   dmaCapabilities DMA capabilities bitmask as described by mmap().
 * @param   dmaFlags    DMA flags bitmask as described by mmap().
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), if cannot map to DMA.
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
        ReportWarning("[MMPS] Buffer already mapped to DMA");

        return MMPS_ALREADY_MAPPED_TO_DMA;
    }

    dmaAddress = mmap(buffer->data,
            buffer->bufferSize,
            dmaCapabilities,
            dmaFlags,
            NOFD,
            0);
    if (dmaAddress == MAP_FAILED)
    {
        ReportError("[MMPS] Cannot map buffer to DMA: errno=%d",
                errno);

        return MMPS_CANNOT_MAP_TO_DMA;
    }

    buffer->dmaAddress = dmaAddress;

    return MMPS_OK;
}

/*
 * @brief   Release single data block from DMA.
 *
 * Release data block of a single MMPS buffer descriptor from DMA.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor those data block
 *                      needs to be unmapped from DMA.
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), if cannot release DMA mapping.
 */
int
MMPS_DMAUnmapBuffer(struct MMPS_Buffer *buffer)
{
    int rc;

    if (buffer->dmaAddress == NULL)
    {
        ReportWarning("[MMPS] Trying to unmap buffer from DMA that was not mapped");

        return MMPS_NOT_MAPPED_TO_DMA;
    }

    rc = munmap(buffer->dmaAddress, buffer->bufferSize);
    if (rc != 0)
    {
        ReportError("[MMPS] Cannot unmap buffer from DMA: errno=%d",
                errno);

        return MMPS_CANNOT_UNMAP_FROM_DMA;
    }

    // Reset DMA address, otherwise this buffer cannot be mapped to DMA again.
    //
    buffer->dmaAddress = NULL;

    return MMPS_OK;
}

#endif // MMPS_DMA

/*
 * @brief   Get buffer for specified MMPS pool and specified buffer bank.
 *
 * @param   pool        Pointer to MMPS pool descriptor.
 * @param   bankId      Bank id.
 * @param   bufferId    Buffer id.
 *
 * @return  Pointer to MMPS buffer descriptor.
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
        ((unsigned long) block +
        (unsigned long) (bufferIdInBlock * sizeof(struct MMPS_Buffer)));

#ifdef MMPS_PEEK_POKE
    ReportDebug("[MMPS] " \
            "... bufferId=%u bufferIdInBlock=%u block=0x%016lX " \
            "buffer=0x%016lX data=0x%016lX",
            bufferId,
            bufferIdInBlock,
            (unsigned long) block,
            (unsigned long) buffer,
            (unsigned long) buffer->data);
#endif

    return buffer;
}

/*
 * @brief   Peek buffer from the first bank of the MMPS pool.
 *
 * @param   pool        Pointer to MMPS pool descriptor.
 * @param   ownerId     Owner id or application id the buffer
 *                      will be associated with.
 *                      This value will be stored in buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor upon successful completion.
 * @return  NULL if there are no free buffers available in a specified pool.
 */
inline struct MMPS_Buffer *
MMPS_PeekBuffer(
    struct MMPS_Pool    *pool,
    const unsigned int  ownerId)
{
    return MMPS_PeekBufferFromBank(pool, 0, ownerId);
}

/*
 * @brief   Peek a buffer of preferred size from of the specified MMPS pool.
 *
 * MMPS will go through all banks until unused buffer is found.
 * If no buffer of a preferred size can be found,
 * a smaller buffer can be returned.
 *
 * @param   pool:       Pointer to MMPS pool descriptor.
 * @param   preferredSize Preferred/minimum size of requested buffer.
 * @param   ownerId:    Owner id or application id the buffer
 *                      will be associated with.
 *                      This value will be stored in buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor upon successful completion.
 * @return  NULL if there are no free buffers available in a specified pool.
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
#ifdef MMPS_USE_OWNER_ID
    ReportDebug("[MMPS] Peek buffer of size %u for 0x%08X", preferredSize, ownerId);
#else
    ReportDebug("[MMPS] Peek buffer of size %u", preferredSize);
#endif
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

            // If there are no free buffers in this bank,
            // then go long with further banks.
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
        ReportWarning("[MMPS] " \
                "No free buffer available " \
                "(requested preferred size %u for 0x%08X)",
                preferredSize,
                ownerId);

        return NULL;
    }

    // For buffers of the bank with the 'alloc on demand' flag set on,
    // allocate memory resources for data each time when the buffer is peeked.
    //
    if (bank->allocateOnDemand == TRUE)
    {
        buffer->data = malloc(buffer->bufferSize);
        if (buffer->data == NULL)
        {
            MMPS_PokeBuffer(buffer);

            ReportSoftAlert("[MMPS] Out of memory");

            return NULL;
        }

        buffer->cursor = buffer->data;
    }

    return buffer;
}

/*
 * @brief   Peek an unused buffer from the specified MMPS bank.
 *
 * @param   pool        Pointer to MMPS pool descriptor.
 * @param   bankId      Bank id to search for unused buffer.
 * @param   ownerId     Owner id or application id the buffer
 *                      will be associated with.
 *                      This value will be stored in buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor upon successful completion.
 * @return  NULL if there are no free buffers available in a specified bank.
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
#ifdef MMPS_USE_OWNER_ID
    ReportDebug("[MMPS] Peek buffer from bank %u for 0x%08X", bankId, ownerId);
#else
    ReportDebug("[MMPS] Peek buffer from bank %u", bankId);
#endif
#endif

    bank = pool->banks[bankId];

#ifdef MMPS_MUTEX
    pthread_mutex_lock(&bank->mutex.lock);
#else
    pthread_spin_lock(&bank->lock);
#endif

    bufferId = bank->ids[bank->cursor.peek];

#ifdef MMPS_PEEK_POKE
    ReportDebug("[MMPS] ... bank->peekCursor=%u bank->pokeCursor=%u bufferId=%u",
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
            pthread_mutex_unlock(&bank->mutex.lock);
#else
            pthread_spin_unlock(&bank->lock);
#endif

            ReportError("[MMPS] Got buffer that already belongs to 0x%08X",
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
    pthread_mutex_unlock(&bank->mutex.lock);
#else
    pthread_spin_unlock(&bank->lock);
#endif

    if (buffer == NULL)
    {
        ReportWarning("[MMPS] " \
                "No free buffer available (requested bank %u for 0x%08X)",
                bankId,
                ownerId);

        return NULL;
    }

    // For buffers of the bank with the 'alloc on demand' flag set on,
    // allocate memory resources for data each time when the buffer is peeked.
    //
    if (bank->allocateOnDemand == TRUE)
    {
        buffer->data = malloc(buffer->bufferSize);
        if (buffer->data == NULL)
        {
            MMPS_PokeBuffer(buffer);

            ReportSoftAlert("[MMPS] Out of memory");

            return NULL;
        }

        buffer->cursor = buffer->data;
    }

    return buffer;
}

/*
 * @brief   Poke buffer or buffer chain back to MMPS pool.
 *
 * In case of chain of buffers, the chain will be disassembled and each buffer
 * will be put back to a corresponding bank.
 *
 * @param   buffer      Pointer to a buffer descriptor or a chain
 *                      of buffer descriptors to be put back to MMPS pool.
 */
void
MMPS_PokeBuffer(struct MMPS_Buffer *buffer)
{
    struct MMPS_Bank    *bank;
    struct MMPS_Buffer  *thisBuffer;
    struct MMPS_Buffer  *nextBuffer;

    thisBuffer = buffer;
    do {
        bank = thisBuffer->bank;

#ifdef MMPS_PEEK_POKE
#ifdef MMPS_USE_OWNER_ID
        ReportDebug("[MMPS] Poke buffer %u to bank %u for 0x%08X",
                thisBuffer->bufferId, bank->bankId, thisBuffer->ownerId);
#else
        ReportDebug("[MMPS] Poke buffer %u to bank %u",
                thisBuffer->bufferId, bank->bankId);
#endif
#endif

#ifdef MMPS_USE_OWNER_ID
        if (thisBuffer->ownerId == MMPS_NO_OWNER)
        {
            ReportError("[MMPS] " \
                    "Trying to poke buffer %u to bank %u that was not peeked",
                    thisBuffer->bufferId,
                    thisBuffer->bank->bankId);

            break;
        }
#endif

        nextBuffer = thisBuffer->next;

        if ((nextBuffer != NULL) && (nextBuffer->prev != thisBuffer))
        {
            ReportError("[MMPS] " \
                    "Detected broken buffer chain for buffers %u and %u",
                    thisBuffer->bufferId,
                    nextBuffer->bufferId);

            nextBuffer = NULL;
        }

        // For buffers of the bank with the 'alloc on demand' flag set on,
        // release memory resources used for data.
        //
        if (bank->allocateOnDemand == TRUE)
        {
            free(thisBuffer->data);
            thisBuffer->data = NULL;
            thisBuffer->cursor = NULL;
        }

        thisBuffer->prev = NULL;
        thisBuffer->next = NULL;
        thisBuffer->dataSize = 0;
        thisBuffer->cursor = thisBuffer->data;
#ifdef MMPS_USE_OWNER_ID
        thisBuffer->ownerId = MMPS_NO_OWNER;
#endif

#ifdef MMPS_MUTEX
        pthread_mutex_lock(&bank->mutex.lock);
#else
        pthread_spin_lock(&bank->lock);
#endif

        bank->ids[bank->cursor.poke] = thisBuffer->bufferId;

        bank->cursor.poke++;
        if (bank->cursor.poke == bank->numberOfBuffers)
            bank->cursor.poke = 0;

#ifdef MMPS_MUTEX
        pthread_mutex_unlock(&bank->mutex.lock);
#else
        pthread_spin_unlock(&bank->lock);
#endif

        thisBuffer = nextBuffer;

#ifdef MMPS_PEEK_POKE
        if (thisBuffer != NULL)
        {
            ReportDebug("[MMPS] ... next buffer %u",
                    thisBuffer->bufferId);
        }
#endif
    } while (thisBuffer != NULL);
}

/*
 * @brief   Increment the number of touches of MMPS buffer.
 *
 * As long the number of touches is more than zero the MMPS buffer
 * could be poked back to the pool.
 *
 * @param   buffer      Pointer to a buffer descriptor or a chain
 *                      of buffer descriptors to be touched.
 */
void
MMPS_TouchBuffer(struct MMPS_Buffer *buffer)
{
#ifdef LINUX

#ifdef MMPS_MUTEX
    pthread_mutex_lock(&buffer->bank->mutex.lock);
#else
    pthread_spin_lock(&buffer->bank->lock);
#endif

    buffer->touches++;

#ifdef MMPS_MUTEX
    pthread_mutex_unlock(&buffer->bank->mutex.lock);
#else
    pthread_spin_unlock(&buffer->bank->lock);
#endif

#else // LINUX

    atomic_add(&buffer->touches, 1);

#endif // LINUX
}

/*
 * @brief   Decrement the number of touches of MMPS buffer.
 *
 * MMPS buffer could be poked back to the pool only when the number of touches
 * is equal zero. When last member absolves the MMPS buffer it is automatically
 * poked back to the pool - in such case there is no need to call
 * MMPS_PokeBuffer().
 *
 * @param   buffer      Pointer to a buffer descriptor or a chain
 *                      of buffer descriptors to be absolved.
 */
void
MMPS_AbsolveBuffer(struct MMPS_Buffer *buffer)
{
#ifdef LINUX

#ifdef MMPS_MUTEX
    pthread_mutex_lock(&buffer->bank->mutex.lock);
#else
    pthread_spin_lock(&buffer->bank->lock);
#endif

    buffer->touches--;

#ifdef MMPS_MUTEX
    pthread_mutex_unlock(&buffer->bank->mutex.lock);
#else
    pthread_spin_unlock(&buffer->bank->lock);
#endif

    if (buffer->touches == 0)
    {
        MMPS_PokeBuffer(buffer);
    }

#else // LINUX

    // Poke the buffer if that was the last touch.
    //
    if (atomic_sub_value(&buffer->touches, 1) == 1)
    {
        MMPS_PokeBuffer(buffer);
    }

#endif // LINUX
}

/*
 * @brief   Get buffer from a chain, which precedes the referencing buffer.
 *
 * @param   buffer      Pointer to the referencing MMPS buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor of the preceding buffer in a chain.
 * @return  NULL if referencing buffer is the first buffer in a chain.
 */
inline struct MMPS_Buffer *
MMPS_PreviousBuffer(struct MMPS_Buffer *buffer)
{
    return buffer->prev;
}

/*
 * @brief   Get buffer from a chain, which succeeds the referencing buffer.
 *
 * @param   buffer      Pointer to the referencing MMPS buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor of the succeeding buffer in a chain.
 * @return  NULL if referencing buffer is the last buffer in a chain.
 */
inline struct MMPS_Buffer *
MMPS_NextBuffer(struct MMPS_Buffer *buffer)
{
    return buffer->next;
}

/*
 * @brief   Get first buffer of a chain.
 *
 * @param   chain       Pointer to the referencing MMPS buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor of the first buffer in a chain.
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

/*
 * @brief   Get last buffer of a chain.
 *
 * @param   chain       Pointer to the referencing MMPS buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor of the last buffer in a chain.
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

/*
 * @brief   Extend a chain with one more buffer.
 *
 * Peek one more buffer from the MMPS pool and append it at the end
 * of a specified chain of buffers. MMPS will try to peek a buffer
 * from the same bank, the referencing buffer belongs to.
 * If there no buffers available in that bank,
 * MMPS will search through all other banks for a free buffer.
 *
 * @param   buffer      Buffer to append a new buffer to.
 *
 * @return  Pointer to MMPS buffer descriptor of the attached buffer.
 *          If referenced buffer did already have a succeeding buffer (chain),
 *          then the pointer to MMPS buffer descriptor of the succeeding buffer
 *          will be returned.
 * @return  NULL if MMPS was not able to find any free buffer in a pool.
 */
struct MMPS_Buffer *
MMPS_ExtendBuffer(struct MMPS_Buffer *origBuffer)
{
    struct MMPS_Buffer *nextBuffer;

    nextBuffer = MMPS_PeekBufferFromBank(
            origBuffer->bank->pool,
            origBuffer->bank->bankId,
#ifdef MMPS_USE_OWNER_ID
            origBuffer->ownerId
#else
            MMPS_NO_OWNER
#endif
    );

    if (nextBuffer == NULL)
    {
        nextBuffer = MMPS_PeekBufferOfSize(
                origBuffer->bank->pool,
                origBuffer->bufferSize,
#ifdef MMPS_USE_OWNER_ID
                origBuffer->ownerId
#else
                MMPS_NO_OWNER
#endif
        );
    }

    origBuffer->next = nextBuffer;
    nextBuffer->prev = origBuffer;

    return nextBuffer;
}

/*
 * @brief   Append a buffer at the end of a chain.
 *
 * Append buffer or a chain of buffers to another buffer
 * or another chain of buffers.
 *
 * @param   destination Pointer to MMPS buffer descriptor from the chain
 *                      to append new buffer to.
 * @param   appendage   Pointer to MMPS buffer descriptor
 *                      that has to be appended at the end of a chain.
 *
 * @return  If a valid pointer to MMPS buffer descriptor is provided
 *          as destination reference, then the same pointer is returned.
 *          If destination is referenced as NULL, then the appendage reference
 *          is returned.
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

/*
 * @brief   Remove buffer from a chain of buffers.
 *
 * @param   anchor      Pointer to MMPS buffer chan anchor.
 * @param   removal     Pointer to MMPS buffer to be removed from a chain.
 *
 * @return  Pointer to MMPS buffer descriptor that is a new anchor of a chain.
 */
struct MMPS_Buffer *
MMPS_RemoveFromChain(
    struct MMPS_Buffer *anchor,
    struct MMPS_Buffer *removal)
{
    struct MMPS_Buffer *newAnchor;

    if (anchor == removal)
    {
        // MMPS buffer to be removed from the chain is actually the chain anchor.
        // Set new chain anchor .

        newAnchor = removal->next;

        if (removal->next != NULL)
        {
            removal->next->prev = NULL;
            removal->next = NULL;
        }

        return newAnchor;
    }
    else
    {
        if (removal->prev != NULL)
        {
            removal->prev->next = removal->next;
            removal->prev = NULL;
        }

        if (removal->next != NULL)
        {
            removal->next->prev = removal->prev;
            removal->next = NULL;
        }

        return anchor;
    }
}

/*
 * @brief   Truncate buffers at the end of a chain.
 *
 * Cut buffers chained on a referenced buffer and give the cut buffers
 * back to the pool. All buffers, succeeding the referenced buffer,
 * will be removed from the chain and put back to the MMPS pool.
 * If referenced buffer is the first buffer in a chain, then after completion
 * of MMPS_TruncateBuffer() this buffer will be a standalone buffer
 * (not chained buffer). If referenced buffer is not the first buffer
 * in a chain, then after completion of MMPS_TruncateBuffer() this buffer
 * will be the last buffer of a chain.
 *
 * @param   tailingBuffer   Pointer to MMPS buffer descriptor of the buffer,
 *                          which must be the tailing buffer after the trancate.
 */
void
MMPS_TruncateChain(struct MMPS_Buffer *tailingBuffer)
{
    struct MMPS_Buffer  *bufferToPurge;

    bufferToPurge = MMPS_NextBuffer(tailingBuffer);
    if (bufferToPurge != NULL)
    {
        tailingBuffer->next = NULL;
        bufferToPurge->prev = NULL;
        MMPS_PokeBuffer(bufferToPurge);
    }
}

/*
 * @brief   Copy data from one buffer chain to another.
 *
 * If destination chain of buffers is smaller than the source
 * then only a part of data will be copied.
 *
 * @param   destination Pointer to MMPS buffer descriptor
 *                      of the destination buffer chain.
 * @param   source      Pointer to MMPS buffer descriptor
 *                      of the source buffer chain.
 *
 * @return  Number of bytes being copied.
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
        // to the next buffer in a destination chain.
        // If there are no other buffers in a destination chain
        // then the job is done
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

        destination->dataSize += bytesToCopy;

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

/*
 * @brief   Get total data size summarized for all buffers in a chain.
 *
 * The buffers in a chain may belong to different banks and be of different size.
 *
 * @param   firstBuffer Pointer to MMPS buffer descriptor of the first buffer
 *                      in a chain to begin counting from.
 *
 * @return  Total data size in bytes.
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

/*
 * @brief   Reset data size to 0.
 *
 * Reset data size to 0 for referencing buffer or, if a chain of buffers
 * is referenced, for all buffers in a chain.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor or anchor
 *                      of a chain of buffers.
 */
inline void
MMPS_ResetBufferData(struct MMPS_Buffer *buffer)
{
    while (buffer != NULL)
    {
        buffer->cursor = buffer->data;
        buffer->dataSize = 0;
        buffer = buffer->next;
    }
}

/*
 * @brief   Reset cursor to the beginning of data.
 *
 * Reset cursor to position to the beginning of data for referencing buffer or,
 * if a chain of buffers is referenced, for all buffers in a chain.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 */
inline void
MMPS_ResetCursor(struct MMPS_Buffer *buffer)
{
    while (buffer != NULL)
    {
        buffer->cursor = buffer->data;
        buffer = buffer->next;
    }
}

/*
 * @brief   Move cursor forwards for specified amount of bytes.
 *
 * If new absolute offset is located in another buffer of a chain then switch
 * from the referencing buffer to succeeding buffers through the chain
 * until a proper buffer is reached.
 *
 * @param   buffer          Pointer to MMPS buffer descriptor.
 * @param   relativeOffset  Number of bytes to move cursor forwards.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer where the cursor
 *          is positioned after the move operation. If new the absolute
 *          position, calculated by current cursor position and relative offset,
 *          is out of range of the referencing buffer or chain of buffers,
 *          then a NULL pointer is returned.
 */
struct MMPS_Buffer *
MMPS_MoveCursorRelative(
    struct MMPS_Buffer  *buffer,
    unsigned int        relativeOffset)
{
    int bytesThatWouldFit;

    // Find out how many bytes would fit in the current buffer.
    //
    bytesThatWouldFit =
        buffer->dataSize - (buffer->cursor - buffer->data) - relativeOffset;

    // Delta equal 0 means that the new cursor position would point to the
    // beginning of next buffer. Extend the chain by one buffer if needed.
    // Delta greater than 0 means that the new data will fit
    // in the current buffer and there will be still free space left.
    // Negative value shows how many bytes would not fit in the current buffer.
    // In such case the chain has to be extended.
    //
    if (bytesThatWouldFit == 0)
    {
        if (buffer->next != NULL)
        {
            buffer = buffer->next;

            buffer->cursor = buffer->data;
        }
        else
        {
            buffer = MMPS_ExtendBuffer(buffer);
        }

        return buffer;
    }
    else if (bytesThatWouldFit > 0)
    {
        buffer->cursor += relativeOffset;

        return buffer;
    }
    else
    {
        relativeOffset -= buffer->dataSize - (buffer->cursor - buffer->data);

        if (buffer->next != NULL)
        {
            buffer = buffer->next;
        }
        else
        {
            buffer = MMPS_ExtendBuffer(buffer);
        }

        buffer = MMPS_MoveCursorRelative(buffer, relativeOffset);

        return buffer;
    }
}

/*
 * @brief   Find out whether cursor is pointing to the end of data.
 *
 * Only current buffer is examined - not the other buffers in a chain.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 *
 * @return  Boolean true if cursor is pointing to the last byte, or false if not.
 */
boolean
MMPS_IsCursorAtTheEndOfData(struct MMPS_Buffer *buffer)
{
    // TODO

    if (buffer->next)
        return false;

    if (buffer->cursor < (buffer->data + buffer->dataSize)) {
        return false;
    } else {
        return true;
    }
}

/*
 * @brief   Put (write) data to a buffer chain under cursor.
 *
 * Append new buffer if necessary.
 *
 * @param   buffer          Pointer to MMPS buffer descriptor.
 * @param   sourceData      Pointer to memory area to copy data from.
 * @param   sourceDataSize  Size of data to copy.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after write is complete.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutData(
    struct MMPS_Buffer  *buffer,
    const char* const   sourceData,
    unsigned int        sourceDataSize)
{
    unsigned int    sourceDataOffset;
    int             freeSpaceInBuffer;
    int             bytesPerBuffer;

    sourceDataOffset = 0;
    while (sourceDataOffset < sourceDataSize)
    {
        freeSpaceInBuffer =
                buffer->bufferSize - (buffer->cursor - buffer->data);

        // Calculate the amount of data to be written to the current buffer.
        //
        bytesPerBuffer = sourceDataSize - sourceDataOffset;
        if (bytesPerBuffer > freeSpaceInBuffer)
            bytesPerBuffer = freeSpaceInBuffer;

        // Copy data or a portion of data that may fit in current buffer.
        //
        memcpy(
            buffer->cursor,
            sourceData + sourceDataOffset,
            bytesPerBuffer);

        buffer->cursor += bytesPerBuffer;

        if (buffer->dataSize < (buffer->cursor - buffer->data + sizeof(char)))
            buffer->dataSize = buffer->cursor - buffer->data;

        sourceDataOffset += bytesPerBuffer;

        // If free space in current buffer was not enough
        // to write complete data ...
        //
        if (sourceDataOffset < sourceDataSize)
        {
            // ... then switch to next buffer in a chain
            // or append a new buffer if necessary.
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

            MMPS_ResetCursor(buffer);
        }
    }

    return buffer;
}

/*
 * @brief   Get (read) data from a buffer chain under cursor.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   destData    Pointer to memory area to copy data to.
 * @param   destDataSize Size of data to copy.
 * @param   bytesCopied If not NULL the it points to location
 *                      in user memory space where the actually copied
 *                      amount of bytes should be stored.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after read is complete.
 * @return  NULL in case the last byte of the last buffer of a chain
 *          has been read.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetData(
    struct MMPS_Buffer  *buffer,
    char* const         destData,
    unsigned int        destDataSize,
    unsigned int        *bytesCopied)
{
    unsigned int    destDataOffset;
    int             bytesPerBuffer;
    int             leftSpaceInBuffer;

    if (bytesCopied != NULL)
        *bytesCopied = 0;

    destDataOffset = 0;
    while (destDataOffset < destDataSize)
    {
        leftSpaceInBuffer = buffer->dataSize - (buffer->cursor - buffer->data);

        // Calculate the amount of data to be read from the current buffer.
        //
        bytesPerBuffer = destDataSize - destDataOffset;
        if (bytesPerBuffer > leftSpaceInBuffer)
            bytesPerBuffer = leftSpaceInBuffer;

        // Copy data or a portion of data from current buffer.
        //
        memcpy(destData + destDataOffset, buffer->cursor, bytesPerBuffer);

        // Notify the amount of copied bytes.
        //
        if (bytesCopied != NULL)
            *bytesCopied += bytesPerBuffer;

        buffer->cursor += bytesPerBuffer;

        destDataOffset += bytesPerBuffer;

        // In case more data has to be read from buffer chain ...
        //
        if (destDataOffset < destDataSize)
        {
            // ... then switch to next buffer in a chain.
            // Quit, if not all data was read and there are no other buffers
            // in a chain.
            //
            if (buffer->next == NULL)
                break;

            // Switch to next buffer.
            //
            buffer = buffer->next;

            MMPS_ResetCursor(buffer);
        }
    }

    if (MMPS_IsCursorAtTheEndOfData(buffer) == false) {
        return buffer;
    } else {
        return MMPS_NextBuffer(buffer);
    }
}

/*
 * @brief   Put one byte to buffer chain under cursor.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   sourceData  Pointer to memory area to copy data from.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after write is complete.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutInt8(
    struct MMPS_Buffer  *buffer,
    uint8               *sourceData)
{
    buffer = MMPS_PutData(buffer, (char *) sourceData, sizeof(uint8));
    return buffer;
}

/*
 * @brief   Get one byte from buffer chain under cursor.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   destData    Pointer to memory area to copy data to.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after read is complete.
 * @return  NULL in case the last byte of the last buffer of a chain
 *          has been read.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetInt8(
    struct MMPS_Buffer  *buffer,
    uint8               *destData)
{
    buffer = MMPS_GetData(buffer, (char *) destData, sizeof(uint8), NULL);
    return buffer;
}

/*
 * @brief   Put one word (two bytes) to buffer chain under cursor.
 *
 * Do endian conversion if needed.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   sourceData  Pointer to memory area to copy data from.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after write is complete.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
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

/*
 * @brief   Get one word (two bytes) from buffer chain under cursor.
 *
 * Do endian conversion if needed.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   destData    Pointer to memory area to copy data to.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after read is complete.
 * @return  NULL in case the last byte of the last buffer of a chain
 *          has been read.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetInt16(
    struct MMPS_Buffer  *buffer,
    uint16              *destData)
{
    uint16 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value), NULL);
    *destData = be16toh(value);
    return buffer;
}

/*
 * @brief   Put one double word (four bytes) to buffer chain under cursor.
 *
 * Do endian conversion if needed
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   sourceData  Pointer to memory area to copy data from.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after write is complete.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
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

/*
 * @brief   Get one double word (four bytes) from buffer chain under cursor.
 *
 * Do endian conversion if needed.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   destData    Pointer to memory area to copy data to.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after read is complete.
 * @return  NULL in case the last byte of the last buffer of a chain
 *          has been read.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetInt32(
    struct MMPS_Buffer  *buffer,
    uint32              *destData)
{
    uint32 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value), NULL);
    *destData = be32toh(value);
    return buffer;
}

/*
 * @brief   Put one quad word (eight bytes) to buffer chain under cursor.
 *
 * Do endian conversion if needed.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   sourceData  Pointer to memory area to copy data from.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after write is complete.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
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
 * @brief   Get one quad word (eight bytes) from buffer chain under cursor.
 *
 * Do endian conversion if needed.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   destData    Pointer to memory area to copy data to.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after read is complete.
 * @return  NULL in case the last byte of the last buffer of a chain
 *          has been read.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetInt64(
    struct MMPS_Buffer  *buffer,
    uint64              *destData)
{
    uint64 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value), NULL);
    *destData = be64toh(value);
    return buffer;
}

#if 0
/*
 * @brief   Put one 32-bit floating-point value (four bytes)
 *
 * Put one 32-bit floating-point value (four bytes)
 * to buffer chain under cursor. Do endian conversion if needed.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   sourceData  Pointer to memory area to copy data from.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after write is complete.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutFloat32(
    struct MMPS_Buffer  *buffer,
    float32             *sourceData)
{
    float32 value = (float32) htobe32(*sourceData);
    buffer = MMPS_PutData(buffer, (char *) &value, sizeof(value));
    return buffer;
}

/*
 * @brief   Get one 32-bit floating-point value (four bytes)
 *
 * Get one 32-bit floating-point value (four bytes)
 * from buffer chain under cursor. Do endian conversion if needed.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   destData    Pointer to memory area to copy data to.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after read is complete.
 * @return  NULL in case the last byte of the last buffer of a chain
 *          has been read.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetFloat32(
    struct MMPS_Buffer  *buffer,
    float32             *destData)
{
    float32 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value), NULL);
    value = (float32) be64toh((uint32) value);
    *destData = value;
    return buffer;
}

/*
 * @brief   Put one 64-bit double precision floating-point value (eight bytes)
 *
 * Put one 64-bit double precision floating-point value (eight bytes)
 * to buffer chain under cursor. Do endian conversion if needed.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   sourceData  Pointer to memory area to copy data from.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after write is complete.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutFloat64(
    struct MMPS_Buffer  *buffer,
    float64             *sourceData)
{
    float64 value = (float64) htobe64(*sourceData);
    buffer = MMPS_PutData(buffer, (char *) &value, sizeof(value));
    return buffer;
}

/*
 * @brief   Get one 64-bit double precision floating-point value (eight bytes)
 *
 * Get one 64-bit double precision floating-point value (eight bytes)
 * from buffer chain under cursor. Do endian conversion if needed.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   destData    Pointer to memory area to copy data to.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after read is complete.
 * @return  NULL in case the last byte of the last buffer of a chain
 *          has been read.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_GetFloat64(
    struct MMPS_Buffer  *buffer,
    float64             *destData)
{
    float64 value;
    buffer = MMPS_GetData(buffer, (char *) &value, sizeof(value), NULL);
    value = (float64) be64toh((uint64) value);
    *destData = value;
    return buffer;
}
#endif

/*
 * @brief   Put string.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 * @param   string      Pointer to a string.
 * @param   length      Length of string.
 *
 * @return  Pointer to MMPS buffer descriptor of a buffer in which a cursor
 *          is pointing to, after write is complete.
 *
 * @warning Always care about return value - it may be another buffer
 *          than a referenced buffer.
 */
struct MMPS_Buffer *
MMPS_PutString(
    struct MMPS_Buffer  *buffer,
    char                *string,
    unsigned int        length)
{
    uint32 swapped = htobe32(length);
    buffer = MMPS_PutData(buffer, (char *)&swapped, sizeof(swapped));
    if (length != 0)
        buffer = MMPS_PutData(buffer, string, length);
    return buffer;
}

/**
 * @brief   Get number of used MMPS buffers for the specified buffer bank.
 *
 * This function may be used for debugging purposes to check
 * periodically how many MMPS buffers are in use (peeked out of the bank).
 *
 * @param   pool        Pointer to MMPS pool descriptor.
 *                      to get statistical information for.
 * @param   bankId      Buffer bank id.
 *
 * @return  The number of buffers 'in use'.
 *
 * @warning Do not use this function in a production code
 *          as it may cause performance degradation.
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
    pthread_mutex_lock(&bank->mutex.lock);
#else
    pthread_spin_lock(&bank->lock);
#endif

    buffersInUse = (bank->cursor.peek >= bank->cursor.poke)
        ? bank->cursor.peek - bank->cursor.poke
        : bank->numberOfBuffers - (bank->cursor.poke - bank->cursor.peek);

#ifdef MMPS_MUTEX
    pthread_mutex_unlock(&bank->mutex.lock);
#else
    pthread_spin_unlock(&bank->lock);
#endif

    return buffersInUse;
}
