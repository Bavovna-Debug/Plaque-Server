#pragma once

/**
 * @version 161115
 *
 * @brief   Mutual Memory Pool System.
 *
 * @code
 * Compile options:
 *
 *   MMPS_MUTEX
 *     If set, then mutex is used for synchronizing the access to the MMPS.
 *     Otherwise, spinlock is used.
 *
 *   MMPS_USE_OWNER_ID
 *     Use buffer owner management. When MMPS buffer is peeked from the pool
 *     the peeking member must specify his id that will be set as owner id
 *     for the buffer. Ownership of MMPS buffer could be changed as long
 *     buffer is moved from one member to another. Owner id makes it easier
 *     to find out which member owns it at some particular point of time.
 *
 *   MMPS_USE_TOUCHING
 *     Implement mechanism of touch counter. MMPS buffer could be touched
 *     and touched by several members. MMPS buffer could be poked
 *     back to the pool only when it is not touched by any member.
 *
 *   MMPS_EYECATCHER
 *     If set, then each MMPS descriptor will begin with an eye catcher,
 *     which may help analysing memory contents during debugging.
 *
 *   MMPS_USE_64BIT_MMAP
 *     If set, mmap64() instead of mmap() will be used to map memory blocks.
 *
 *   MMPS_SHM
 *     If set, then using shared memory data blocks will be enabled.
 *
 *   MMPS_DMA
 *     If set, then DMA specific functions will be included to the API.
 *
 *   MMPS_INIT_BANK
 *     If set, then verbose information about initialization
 *     of MMPS descriptors will be printed (debugging and performance
 *     analysis purpose).
 *
 *   MMPS_INIT_BANK_DEEP
 *     If set, then more verbose information about initialization
 *     of MMPS descriptors will be printed.
 *
 *   MMPS_PEEK_POKE
 *     If set, then verbose information about MMPS buffer peek and poke flow
 *     will be printed (debugging and performance analysis purpose).
 * @endcode
 */

/**
 * @verbatim
 +---------------------------------------------------------------------+
 |                              MMPS Pool                              |
 +---------------------------------------------------------------------+
                ||                                       ||
                \/                                       \/
 +-------------------------------+     +-------------------------------+
 |           MMPS Bank           | ... |           MMPS Bank           |
 +-------------------------------+     +-------------------------------+
       ||                 ||                 ||                 ||
       \/                 \/                 \/                 \/
 +------------+     +------------+     +------------+     +------------+
 |    MMPS    |     |    MMPS    |     |    MMPS    |     |    MMPS    |
 |   Buffer   |     |   Buffer   |     |   Buffer   |     |   Buffer   |
 +------------+ ... +------------+     +------------+ ... +------------+
 |   * data   |     |   * data   |     |   * data   |     |   * data   |
 +------------+     +------------+     +------------+     +------------+
       ||                 ||                 ||                 ||
       \/                 \/                 \/                 \/
   +--------+         +--------+         +--------+         +--------+
   | memory |         | memory |         | memory |         | memory |
   +--------+         +--------+         +--------+         +--------+
 * @endverbatim
 */

// System definition files.
//
#include <pthread.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "Types.h"

#define MAX_BLOCK_SIZE                  MB

#define MMPS_NO_OWNER                   0

#define MMPS_OK                          0
#define MMPS_OUT_OF_MEMORY              -1
#define MMPS_WRONG_BANK_ID              -100
#define MMPS_SHM_ERROR                  -200
#define MMPS_CANNOT_MAP_TO_SHM          -201
#define MMPS_CANNOT_UNMAP_FROM_SHM      -202
#define MMPS_ALREADY_MAPPED_TO_SHM      -203
#define MMPS_NOT_MAPPED_TO_SHM          -204
#define MMPS_CANNOT_MAP_TO_DMA          -301
#define MMPS_CANNOT_UNMAP_FROM_DMA      -302
#define MMPS_ALREADY_MAPPED_TO_DMA      -303
#define MMPS_NOT_MAPPED_TO_DMA          -304

#ifdef MMPS_EYECATCHER
#define EYECATCHER_SIZE                 16
#define EYECATCHER_POOL                 "<<<< POOL >>>>"
#define EYECATCHER_BANK                 "<<<< BANK >>>>"
#define EYECATCHER_BUFFER               "<<< BUFFER >>>"
#define EYECATCHER_NETVECTOR            "<<< VECTOR >>>"
#endif

/**
 * MMPS pool descriptor.
 */
struct MMPS_Pool
{
#ifdef MMPS_EYECATCHER
    char                eyeCatcher[EYECATCHER_SIZE];
#endif

    /**
     * Number of banks in a pool.
     */
    unsigned int        numberOfBanks;

    /**
     * Internally used array of pointers to banks.
     */
    struct MMPS_Bank    *banks[];
};

/**
 * MMPS bank descriptor.
 */
struct MMPS_Bank
{
#ifdef MMPS_EYECATCHER
    char                eyeCatcher[EYECATCHER_SIZE];
#endif

    /**
     * Bank id to be used internally by this module.
     */
    unsigned int        bankId;

    /**
     * If 'allocate on demand' is true, then data buffers should be allocated
     * on peek and released on poke.
     */
    boolean             allocateOnDemand;

    /**
     * Pointer to the pool the bank belongs to.
     */
    struct MMPS_Pool    *pool;

    /**
     * Lock to be held for each operation on a bank
     * that may cause change in buffers order.
     */
    union
    {
        struct
        {
            pthread_mutexattr_t attr;
            pthread_mutex_t     lock;
        } mutex;

        pthread_spinlock_t  lock;
    };

    /**
     * Shared memory file descriptor. This field is used only
     * if buffers of the bank were allocated using shared memory.
     */
    int                 sharedMemoryHandle;
    void                *sharedMemoryAddress;

    /**
     * Total number of buffers in a bank. All buffers must have the same size.
     * Several banks have to be defines in case buffers of different sizes
     * are required.
     */
    unsigned int        numberOfBuffers;

    /**
     * Specifies the maximum amount of data that may fit in a buffer.
     * This value may be used to find out whether this bank is suitable
     * to be used to store data of some particular size.
     */
    unsigned int        bufferSize;

    /**
     * Size of the follower - an optional piece of data that could be used
     * to provide a kind of object description or header for dnetwork datagrams.
     */
    unsigned int        followerSize;

    /**
     * Internally used cursor pointing to a buffer that can be taken
     * from a queue on next peek operation, and a buffer that was put back
     * to a queue on last poke operation.
     */
    struct
    {
        unsigned int    peek;
        unsigned int    poke;
    } cursor;

    /**
     * Internally used values that are needed only for destructor
     * (to release allocated ressources).
     */
    unsigned int        numberOfBlocks;
    size_t              eachBlockSize;
    size_t              lastBlockSize;
    unsigned int        buffersPerBlock;
    void                **blocks;

    /**
     * Internally used queue of "free" buffers.
     */
    unsigned int        ids[];
};

/**
 * MMPS buffer descriptor.
 */
struct MMPS_Buffer
{
#ifdef MMPS_EYECATCHER
    char                eyeCatcher[EYECATCHER_SIZE];
#endif

    /**
     * Buffer id to be used internally by this module.
     */
    unsigned int        bufferId;

    /**
     * Pointer to the bank the buffer belongs to.
     */
    struct MMPS_Bank    *bank;

    /**
     * Chaining information in case buffer is part of a chain.
     *   prev: Previous buffer in a chain or NULL for the first buffer.
     *   next: Next buffer in a chain or NULL for the last buffer.
     * For non-chained buffers both prev and next are set to NULL.
     */
    struct MMPS_Buffer  *prev;
    struct MMPS_Buffer  *next;

    /**
     * User defined 32-bit value specified who is using this buffer.
     */
    unsigned int        ownerId;

    /**
     * Number of touches. Shows how many members have buffer in touch.
     */
    unsigned            touches;

    /**
     * Specifies the maximum amount of data that may fit in a buffer.
     */
    unsigned int        bufferSize;

    /**
     * Amount of data actually being written in a buffer.
     * 0 <= dataSize <= bufferSize
     */
    unsigned int        dataSize;

    /**
     * Size of the follower - an optional piece of data that could be used
     * to provide a kind of object description or header for dnetwork datagrams.
     */
    unsigned int        followerSize;

    /**
     * Pointer to data memory block.
     */
    char                *data;

    /**
     * Pointer to follower.
     */
    char                *follower;

    /**
     * DMA address of data memory block if it was mapped for DMA,
     * otherwise null.
     */
    char                *dmaAddress;

    /**
     * Pointer, that is used as a cursor in a data memory block.
     * Each time a buffer is peeked from the bank the cursor is reset
     * to the beginning of data.
     */
    char                *cursor;
};

/**
 * MMPS network vector descriptor.
 */
struct MMPS_NetVector
{
#ifdef MMPS_EYECATCHER
    char                eyeCatcher[EYECATCHER_SIZE];
#endif

    struct msghdr       message;
    unsigned int        maximalNumberOfVectors;
    unsigned int        currentNumberOfVectors;
    struct iovec        iov[];
};

/**
 * @brief   Allocate and initialize buffer pool.
 *
 * @param   numberOfBanks   Number of banks to allocate.
 *                          Each bank has to be initialized
 *                          afterwards using MMPS_InitBank().
 *
 * @return  Pointer to MMPS pool descriptor upon successful completion.
 * @return  NULL if MMPS pool cannot be created.
 */
extern struct MMPS_Pool *
MMPS_InitPool(const unsigned int numberOfBanks);

/**
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
extern int
MMPS_InitBank(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    unsigned int        bufferSize,
    unsigned int        followerSize,
    unsigned int        numberOfBuffers);

/**
 * @brief   Allocate data memory blocks for all buffers of the specified bank.
 *
 * @param   pool        Pointer to MMPS pool descriptor - already initialized
 *                      with MMPS_InitPool().
 * @param   bankId      Bank id of the bank those buffers to allocate.
 *
 * @return  0 upon successful completion.
 * @return  Error code (negative value), in case of error.
 */
extern int
MMPS_AllocateImmediately(
    struct MMPS_Pool    *pool,
    unsigned int        bankId);

/**
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
    unsigned int        bankId);

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
    unsigned int        bankId);

/**
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
extern int
MMPS_MapShMemBufferBank(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    const char          *sharedMemoryName);

/**
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
extern int
MMPS_MapShMemBuffer(struct MMPS_Buffer *buffer);

/**
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
extern int
MMPS_UnmapShMemBuffer(struct MMPS_Buffer *buffer);

#ifdef MMPS_DMA

/**
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
extern int
MMPS_DMAMapBufferBank(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    int                 dmaCapabilities,
    int                 dmaFlags);

/**
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
extern int
MMPS_DMAMapBuffer(
    struct MMPS_Buffer  *buffer,
    int                 dmaCapabilities,
    int                 dmaFlags);

/**
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
extern int
MMPS_DMAUnmapBuffer(struct MMPS_Buffer *buffer);

#endif // MMPS_DMA

/**
 * @brief   Get buffer for specified MMPS pool and specified buffer bank.
 *
 * @param   pool        Pointer to MMPS pool descriptor.
 * @param   bankId      Bank id.
 * @param   bufferId    Buffer id.
 *
 * @return  Pointer to MMPS buffer descriptor.
 */
extern struct MMPS_Buffer *
MMPS_BufferById(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    unsigned int        bufferId);

/**
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
extern struct MMPS_Buffer *
MMPS_PeekBuffer(
    struct MMPS_Pool    *pool,
    const unsigned int  ownerId);

/**
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
extern struct MMPS_Buffer *
MMPS_PeekBufferOfSize(
    struct MMPS_Pool    *pool,
    unsigned int        preferredSize,
    const unsigned int  ownerId);

/**
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
extern struct MMPS_Buffer *
MMPS_PeekBufferFromBank(
    struct MMPS_Pool    *pool,
    unsigned int        bankId,
    const unsigned int  ownerId);

/**
 * @brief   Poke buffer or buffer chain back to MMPS pool.
 *
 * In case of chain of buffers, the chain will be disassembled and each buffer
 * will be put back to a corresponding bank.
 *
 * @param   buffer      Pointer to a buffer descriptor or a chain
 *                      of buffer descriptors to be put back to MMPS pool.
 */
extern void
MMPS_PokeBuffer(struct MMPS_Buffer *buffer);

/**
 * @brief   Increment the number of touches of MMPS buffer.
 *
 * As long the number of touches is more than zero the MMPS buffer
 * could be poked back to the pool.
 *
 * @param   buffer      Pointer to a buffer descriptor or a chain
 *                      of buffer descriptors to be touched.
 */
extern void
MMPS_TouchBuffer(struct MMPS_Buffer *buffer);

/**
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
extern void
MMPS_AbsolveBuffer(struct MMPS_Buffer *buffer);

/**
 * @brief   Get buffer from a chain, which precedes the referencing buffer.
 *
 * @param   buffer      Pointer to the referencing MMPS buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor of the preceding buffer in a chain.
 * @return  NULL if referencing buffer is the first buffer in a chain.
 */
extern struct MMPS_Buffer *
MMPS_PreviousBuffer(struct MMPS_Buffer *buffer);

/**
 * @brief   Get buffer from a chain, which succeeds the referencing buffer.
 *
 * @param   buffer      Pointer to the referencing MMPS buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor of the succeeding buffer in a chain.
 * @return  NULL if referencing buffer is the last buffer in a chain.
 */
extern struct MMPS_Buffer *
MMPS_NextBuffer(struct MMPS_Buffer *buffer);

/**
 * @brief   Get first buffer of a chain.
 *
 * @param   chain       Pointer to the referencing MMPS buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor of the first buffer in a chain.
 */
extern struct MMPS_Buffer *
MMPS_FirstBuffer(struct MMPS_Buffer *chain);

/**
 * @brief   Get last buffer of a chain.
 *
 * @param   chain       Pointer to the referencing MMPS buffer descriptor.
 *
 * @return  Pointer to MMPS buffer descriptor of the last buffer in a chain.
 */
extern struct MMPS_Buffer *
MMPS_LastBuffer(struct MMPS_Buffer *chain);

/**
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
extern struct MMPS_Buffer *
MMPS_ExtendBuffer(struct MMPS_Buffer *origBuffer);

/**
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
extern struct MMPS_Buffer *
MMPS_AppendBuffer(
    struct MMPS_Buffer *destination,
    struct MMPS_Buffer *appendage);

/**
 * @brief   Remove buffer from a chain of buffers.
 *
 * @param   anchor      Pointer to MMPS buffer chan anchor.
 * @param   removal     Pointer to MMPS buffer to be removed from a chain.
 *
 * @return  Pointer to MMPS buffer descriptor that is a new anchor of a chain.
 */
extern struct MMPS_Buffer *
MMPS_RemoveFromChain(
    struct MMPS_Buffer *anchor,
    struct MMPS_Buffer *removal);

/**
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
extern void
MMPS_TruncateChain(struct MMPS_Buffer *tailingBuffer);

/**
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
extern unsigned int
MMPS_CopyBuffer(struct MMPS_Buffer *destination, struct MMPS_Buffer *source);

/**
 * @brief   Get total data size summarized for all buffers in a chain.
 *
 * The buffers in a chain may belong to different banks and be of different size.
 *
 * @param   firstBuffer Pointer to MMPS buffer descriptor of the first buffer
 *                      in a chain to begin counting from.
 *
 * @return  Total data size in bytes.
 */
extern unsigned int
MMPS_TotalDataSize(struct MMPS_Buffer *firstBuffer);

/**
 * @brief   Reset data size to 0.
 *
 * Reset data size to 0 for referencing buffer or, if a chain of buffers
 * is referenced, for all buffers in a chain.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor or anchor
 *                      of a chain of buffers.
 */
extern void
MMPS_ResetBufferData(struct MMPS_Buffer *buffer);

/**
 * @brief   Reset cursor to the beginning of data.
 *
 * Reset cursor to position to the beginning of data for referencing buffer or,
 * if a chain of buffers is referenced, for all buffers in a chain.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 */
extern void
MMPS_ResetCursor(struct MMPS_Buffer *buffer);

/**
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
extern struct MMPS_Buffer *
MMPS_MoveCursorRelative(
    struct MMPS_Buffer  *buffer,
    unsigned int        relativeOffset);

/**
 * @brief   Find out whether cursor is pointing to the end of data.
 *
 * Only current buffer is examined - not the other buffers in a chain.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 *
 * @return  Boolean true if cursor is pointing to the last byte, or false if not.
 */
extern boolean
MMPS_IsCursorAtTheEndOfData(struct MMPS_Buffer *buffer);

#if 0

/**
 * @brief   Prepare MMPS buffer descriptor for use for network communication.
 *
 * Prepare MMPS buffer descriptor for use for network communication
 * with scatter-gather list.
 *
 * @param   buffer      Pointer to MMPS buffer descriptor.
 *
 * @return  Pointer to the beginning of the scatter-gather list.
 */
extern struct MMPS_Vector *
MMPS_PrepareScatterGather(struct MMPS_Buffer *buffer);

#endif

/**
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
extern struct MMPS_Buffer *
MMPS_PutData(
    struct MMPS_Buffer  *buffer,
    const char* const   sourceData,
    unsigned int        sourceDataSize);

/**
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
extern struct MMPS_Buffer *
MMPS_GetData(
    struct MMPS_Buffer  *buffer,
    char* const         destData,
    unsigned int        destDataSize,
    unsigned int        *bytesCopied);

/**
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
extern struct MMPS_Buffer *
MMPS_PutInt8(
    struct MMPS_Buffer  *buffer,
    uint8               *sourceData);

/**
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
extern struct MMPS_Buffer *
MMPS_GetInt8(
    struct MMPS_Buffer  *buffer,
    uint8               *destData);

/**
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
extern struct MMPS_Buffer *
MMPS_PutInt16(
    struct MMPS_Buffer  *buffer,
    uint16              *sourceData);

/**
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
extern struct MMPS_Buffer *
MMPS_GetInt16(
    struct MMPS_Buffer  *buffer,
    uint16              *destData);

/**
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
extern struct MMPS_Buffer *
MMPS_PutInt32(
    struct MMPS_Buffer  *buffer,
    uint32              *sourceData);

/**
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
extern struct MMPS_Buffer *
MMPS_GetInt32(
    struct MMPS_Buffer  *buffer,
    uint32              *destData);

/**
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
extern struct MMPS_Buffer *
MMPS_PutInt64(
    struct MMPS_Buffer  *buffer,
    uint64              *sourceData);

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
extern struct MMPS_Buffer *
MMPS_GetInt64(
    struct MMPS_Buffer  *buffer,
    uint64              *destData);

#if 0

/**
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
extern struct MMPS_Buffer *
MMPS_PutFloat32(
    struct MMPS_Buffer  *buffer,
    float32             *sourceData);

/**
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
extern struct MMPS_Buffer *
MMPS_GetFloat32(
    struct MMPS_Buffer  *buffer,
    float32             *destData);

/**
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
extern struct MMPS_Buffer *
MMPS_PutFloat64(
    struct MMPS_Buffer  *buffer,
    float64             *sourceData);

/**
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
extern struct MMPS_Buffer *
MMPS_GetFloat64(
    struct MMPS_Buffer  *buffer,
    float64             *destData);

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
extern struct MMPS_Buffer *
MMPS_PutString(
    struct MMPS_Buffer  *buffer,
    char                *string,
    unsigned int        length);

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
extern unsigned int
MMPS_NumberOfBuffersInUse(
    struct MMPS_Pool    *pool,
    unsigned int        bankId);
