#ifndef __MMPS__
#define __MMPS__

/*
 *  +---------------------------------------------------------------------------+
 *  |                              struct mmpsPool                              |
 *  +---------------------------------------------------------------------------+
 *                  ||                                        ||
 *                  \/                                        \/
 *  +---------------------------------+       +---------------------------------+
 *  |         struct mmpsBank         |  ...  |         struct mmpsBank         |
 *  +---------------------------------+       +---------------------------------+
 *        ||                   ||                   ||                   ||
 *        \/                   \/                   \/                   \/
 *  +------------+       +------------+       +------------+       +------------+
 *  |   struct   |       |   struct   |       |   struct   |       |   struct   |
 *  | mmpsBuffer |       | mmpsBuffer |       | mmpsBuffer |       | mmpsBuffer |
 *  +------------+  ...  +------------+       +------------+  ...  +------------+
 *  |   * data   |       |   * data   |       |   * data   |       |   * data   |
 *  +------------+       +------------+       +------------+       +------------+
 *        ||                   ||                   ||                   ||
 *        \/                   \/                   \/                   \/
 *    +--------+           +--------+           +--------+           +--------+
 *    | memory |           | memory |           | memory |           | memory |
 *    +--------+           +--------+           +--------+           +--------+
 */

/**********************************************************************************/
/*                                                                                */
/*  Compile options:                                                              */
/*                                                                                */
/*    MMPS_MUTEX                                                                  */
/*      If set, then mutex is used for synchronizing the access to the MMPS.      */
/*      Otherwise, spinlock is used.                                              */
/*                                                                                */
/*    MMPS_USE_OWNER_ID                                                           */
/*                                                                                */
/*    MMPS_EYECATCHER                                                             */
/*      If set, then each MMPS handler will begin with an eye catcher that may    */
/*      help debugging the problems when memory contents are analysed.            */
/*                                                                                */
/*    MMPS_DMA                                                                    */
/*      If set, then DMA specific functions will be included to the API.          */
/*                                                                                */
/*    MMPS_INIT_BANK                                                              */
/*      If set, then verbose information about initialization of MMPS handlers    */
/*      will be printed (debugging and performance analysis purpose).             */
/*                                                                                */
/*    MMPS_INIT_BANK_DEEP                                                         */
/*      If set, then more verbose information about initialization of MMPS        */
/*      handlers will be printed.                                                 */
/*                                                                                */
/*    MMPS_PEEK_POKE                                                              */
/*      If set, then verbose information about MMPS buffer peek and poke flow     */
/*      will be printed (debugging and performance analysis purpose).             */
/*                                                                                */
/**********************************************************************************/

#include <pthread.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#define KB 1024
#define MB 1024 * 1024
#define GB 1024 * 1024 * 1024

typedef uint8_t             uint8;
typedef int8_t              sint8;
typedef uint16_t            uint16;
typedef int16_t             sint16;
typedef uint32_t            uint32;
typedef int32_t             sint32;
typedef uint64_t            uint64;
typedef int64_t             sint64;

typedef float               float32;
typedef double              float64;

typedef uint8_t             boolean;

#define MAX_BLOCK_SIZE                  MB

#define MMPS_NO_OWNER                   0

#define MMPS_OK                          0
#define MMPS_OUT_OF_MEMORY              -1
#define MMPS_WRONG_BANK_ID              -100
#define MMPS_CANNOT_MAP_TO_DMA          -200
#define MMPS_CANNOT_UNMAP_FROM_DMA      -201
#define MMPS_ALREADY_DMA_MAPPED         -202
#define MMPS_NOT_DMA_MAPPED             -203

#ifdef MMPS_EYECATCHER
#define EYECATCHER_SIZE                 16
#define EYECATCHER_POOL                 "<<<< POOL >>>>"
#define EYECATCHER_BANK                 "<<<< BANK >>>>"
#define EYECATCHER_BUFFER               "<<< BUFFER >>>"
#define EYECATCHER_NETVECTOR            "<<< VECTOR >>>"
#endif

// MMPS pool handler.
//
typedef struct MMPS_Pool
{
#ifdef MMPS_EYECATCHER
    char                eyeCatcher[EYECATCHER_SIZE];
#endif

    // Number of banks in a pool.
    //
    unsigned int        numberOfBanks;

    // Internally used array of pointers to banks.
    //
    struct MMPS_Bank    *banks[];
} pool_t;

// MMPS bank handler.
//
typedef struct MMPS_Bank
{
#ifdef MMPS_EYECATCHER
    char                eyeCatcher[EYECATCHER_SIZE];
#endif

    // Bank id to be used internally by this module.
    //
    unsigned int        bankId;

    // Pointer to the pool the bank belongs to.
    //
    struct MMPS_Pool    *pool;

    // Lock to be held for each operation on a bank that may cause change in buffers order.
    //
#ifdef MMPS_MUTEX
    pthread_mutex_t     mutex;
#else
    pthread_spinlock_t  lock;
#endif

    // Total number of buffers in a bank. All buffers must have the same size.
    // Several banks have to be defines in case buffers of different sizes are needed.
    //
    unsigned int        numberOfBuffers;

    // Specifies the maximum amount of data that may fit in a buffer.
    // This value may be used to find out whether this bank is suitable
    // to be used to store data of some particular size.
    //
    unsigned int        bufferSize;

    // Size of pilot (header block) for use in layered network applications.
    //
    unsigned int        pilotSize;

    // Internally used cursor pointing to a buffer that can be taken from a queue
    // on next peek operation, and a buffer that was put back to a queue
    // on last poke operation.
    //
    struct
    {
        unsigned int    peek;
        unsigned int    poke;
    } cursor;

    // Internally used values that are needed only for destructor
    // (to release allocated ressources).
    //
    unsigned int        numberOfBlocks;
    size_t              eachBlockSize;
    size_t              lastBlockSize;
    unsigned int        buffersPerBlock;
    void                **blocks;

    // Internally used queue of "free" buffers.
    //
    unsigned int        ids[];
} bank_t;

// MMPS buffer handler.
//
typedef struct MMPS_Buffer
{
#ifdef MMPS_EYECATCHER
    char                eyeCatcher[EYECATCHER_SIZE];
#endif

    // Buffer id to be used internally by this module.
    //
    unsigned int        bufferId;

    // Pointer to the bank the buffer belongs to.
    //
    struct MMPS_Bank    *bank;

    // Chaining information in case buffer is part of a chain.
    //
    struct MMPS_Buffer  *prev;
    struct MMPS_Buffer  *next;

    // Specifies the maximum amount of data that may fit in a buffer.
    //
    unsigned int        bufferSize;

    // Size of pilot (header block) for use in layered network applications.
    //
    unsigned int        pilotSize;

    // Amount of data actually being written in a buffer.
    //
    //   0 <= dataSize <= bufferSize
    //
    unsigned int        dataSize;

#ifdef MMPS_USE_OWNER_ID
    //
    // User defined 32-bit value specified who is using this bufer.
    //
    unsigned int        ownerId;
#endif

    // Pointer to a beginning of a data memory block.
    //
    char                *data;

    // DMA address of data memory block if it was mapped for DMA, otherwise null.
    //
    char                *dmaAddress;

    // Pointer, that is used as a cursor in a data memory block.
    // Each time a buffer is peeked from the bank the cursor is reset
    // to the beginning of data.
    //
    char                *cursor;
} buffer_t;

// MMPS buffer handler.
//
typedef struct MMPS_NetVector
{
#ifdef MMPS_EYECATCHER
    char                eyeCatcher[EYECATCHER_SIZE];
#endif

    struct msghdr       message;
    unsigned int        maximalNumberOfVectors;
    unsigned int        currentNumberOfVectors;
    struct iovec        iov[];
} net_vector_t;

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
MMPS_InitPool(const unsigned int numberOfBanks);

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
    unsigned int        numberOfBuffers);

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
    int                 dmaFlags);

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
    int                 dmaFlags);

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
MMPS_DMAUnmapBuffer(struct MMPS_Buffer *buffer);
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
    unsigned int        bufferId);

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
struct MMPS_Buffer *
MMPS_PeekBuffer(
    struct MMPS_Pool    *pool,
    const unsigned int  ownerId);

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
    const unsigned int  ownerId);

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
    const unsigned int  ownerId);

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
MMPS_PokeBuffer(struct MMPS_Buffer *buffer);

/**
 * MMPS_PreviousBuffer()
 * Get buffer from a chain of buffers which precedes the referencing buffer.
 *
 * @buffer: Pointer to the referencing MMPS buffer handler.
 *
 * Returns pointer to MMPS buffer handler of the preceding buffer in a chain
 * or NULL, if referencing buffer is the first buffer in a chain.
 */
struct MMPS_Buffer *
MMPS_PreviousBuffer(struct MMPS_Buffer *buffer);

/**
 * MMPS_NextBuffer()
 * Get buffer from a chain of buffers which succeeds the referencing buffer.
 *
 * @buffer: Pointer to the referencing MMPS buffer handler.
 *
 * Returns pointer to MMPS buffer handler of the succeeding buffer in a chain
 * or NULL, if referencing buffer is the last buffer in a chain.
 */
struct MMPS_Buffer *
MMPS_NextBuffer(struct MMPS_Buffer *buffer);

/**
 * MMPS_FirstBuffer()
 * Get first buffer of a chain.
 *
 * @chain: Pointer to the referencing MMPS buffer handler.
 *
 * Returns pointer to MMPS buffer handler of the first buffer in a chain.
 */
struct MMPS_Buffer *
MMPS_FirstBuffer(struct MMPS_Buffer *chain);

/**
 * MMPS_LastBuffer()
 * Get last buffer of a chain.
 *
 * @chain: Pointer to the referencing MMPS buffer handler.
 *
 * Returns pointer to MMPS buffer handler of the last buffer in a chain.
 */
struct MMPS_Buffer *
MMPS_LastBuffer(struct MMPS_Buffer *chain);

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
MMPS_ExtendBuffer(struct MMPS_Buffer *origBuffer);

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
    struct MMPS_Buffer *appendage);

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
MMPS_TruncateBuffer(struct MMPS_Buffer *buffer);

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
MMPS_CopyBuffer(struct MMPS_Buffer *destination, struct MMPS_Buffer *source);

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
unsigned int
MMPS_TotalDataSize(struct MMPS_Buffer *firstBuffer);

/**
 * MMPS_ResetBufferData()
 * Reset data size to 0 for referencing buffer or, if a chain of buffers is referenced,
 * for all buffers in a chain.
 *
 * @buffer:             Pointer to MMPS buffer handler or anchor of a chain of buffers.
 * @leavePilot:         Boolean value specifying whether pilot has to be left
 *                      in the anchor buffer.
 */
void
MMPS_ResetBufferData(struct MMPS_Buffer *buffer, unsigned int leavePilot);

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
void
MMPS_ResetCursor(struct MMPS_Buffer *buffer, unsigned int leavePilot);

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
    unsigned int        relativeOffset);

/**
 * MMPS_PrepareScatterGather()
 * Prepare MMPS buffer handler for use for network communication
 * with scatter-gather list.
 *
 * @buffer:             Pointer to MMPS buffer handler.
 *
 * Returns pointer to the beginning of the scatter-gather list.
 */
struct MMPS_Vector *
MMPS_PrepareScatterGather(struct MMPS_Buffer *buffer);

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
    unsigned int        sourceDataSize);

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
    unsigned int        destDataSize);

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
    uint8               *sourceData);

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
    uint8               *destData);

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
    uint16              *sourceData);

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
    uint16              *destData);

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
    uint32              *sourceData);

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
    uint32              *destData);

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
    uint64              *sourceData);

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
    uint64              *destData);

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
    float               *sourceData);

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
    float               *destData);

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
    double              *sourceData);

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
    double              *destData);

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
    unsigned int        sourceDataSize);

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
    unsigned int        bankId);

#endif
