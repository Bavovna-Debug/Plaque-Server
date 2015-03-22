#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "paquet.h"
#include "buffers.h"

#define PEEK_RETRIES			3

#define NOTHING					-1

typedef struct chain {
	pthread_spinlock_t	lock;
	int					numberOfBuffers;
	int					peekCursor;
	int					pokeCursor;
	void				*block;
	int					ids[];
} chain_t;

static struct chain *chains[3];

void constructBufferChain(int chainId, int bufferSize, int numberOfBuffers)
{
	struct chain *chain = malloc(sizeof(struct chain) + numberOfBuffers * sizeof(int));
	if (chain == NULL) {
        fprintf(stderr, "No memory\n");
        return;
    }

    chain->block = malloc(numberOfBuffers * sizeof(struct buffer));
	if (chain->block == NULL) {
        fprintf(stderr, "No memory\n");
        return;
    }

	pthread_spin_init(&chain->lock, PTHREAD_PROCESS_PRIVATE);

	chain->numberOfBuffers = numberOfBuffers;
	chain->peekCursor = 0;
	chain->pokeCursor = 0;

	int bufferId;
	for (bufferId = 0; bufferId < chain->numberOfBuffers; bufferId++)
	{
		struct buffer *buffer = chain->block + bufferId * sizeof(struct buffer);

		buffer->bufferId = bufferId;
		buffer->chainId = chainId;
		buffer->next = NULL;
		buffer->bufferSize = bufferSize;
		buffer->dataSize = 0;
    	buffer->data = malloc(bufferSize);
		if (buffer->data == NULL) {
        	fprintf(stderr, "No memory\n");
    	    return;
	    }
		buffer->cursor = buffer->data;
	}

	for (bufferId = 0; bufferId < chain->numberOfBuffers; bufferId++)
		chain->ids[bufferId] = bufferId;

	chains[chainId] = chain;
}

void constructBuffers(void)
{
	constructBufferChain(BUFFER1K, KB, 1000000);
	constructBufferChain(BUFFER4K, 4 * KB, 200000);
	constructBufferChain(BUFFER1M, MB, 200);
}

void destructBuffers(void)
{
}

struct buffer* peekBuffer(int preferredSize)
{
	struct buffer* buffer;

	int chainId = -1;

	int try;
	for (try = 0; try < PEEK_RETRIES; try++)
	{
		if (chainId == -1) {
			if (preferredSize >= MB)
				chainId = BUFFER1M;
			else if (preferredSize >= 4 * KB)
				chainId = BUFFER4K;
			else
				chainId = BUFFER1K;
		} else {
			if (chainId == BUFFER1K)
				chainId = BUFFER4K;
			else if (chainId == BUFFER4K)
				chainId = BUFFER1M;
			else
				chainId = BUFFER1K;
		}

		struct chain *chain = chains[chainId];

		pthread_spin_lock(&chain->lock);

		int bufferId = chain->ids[chain->peekCursor];
		if (bufferId == NOTHING) {
			buffer = NULL;
		} else {
			buffer = chain->block + bufferId * sizeof(struct buffer);

			chain->ids[chain->peekCursor] = NOTHING;

			chain->peekCursor++;
			if (chain->peekCursor == chain->numberOfBuffers)
				chain->peekCursor = 0;
		}

		pthread_spin_unlock(&chain->lock);

		if (buffer != NULL)
			break;
	}

#ifdef DEBUG
	if (buffer == NULL)
		printf("No free buffer available\n");
#endif

	return buffer;
}

void pokeBuffer(struct buffer* buffer)
{
	do {
		struct buffer *nextBuffer = buffer->next;

		buffer->next = NULL;
		buffer->dataSize = 0;
		buffer->cursor = buffer->data;

		struct chain *chain = chains[buffer->chainId];

		pthread_spin_lock(&chain->lock);

		chain->ids[chain->pokeCursor] = buffer->bufferId;

		chain->pokeCursor++;
		if (chain->pokeCursor == chain->numberOfBuffers)
			chain->pokeCursor = 0;

		pthread_spin_unlock(&chain->lock);

		buffer = nextBuffer;
	} while (buffer != NULL);
}

int totalDataSize(struct buffer* firstBuffer)
{
	int totalDataSize = 0;
	struct buffer *buffer;
	for (buffer = firstBuffer; buffer != NULL; buffer = buffer->next)
		totalDataSize += buffer->dataSize;

	return totalDataSize;
}

void resetBufferData(struct buffer* buffer, int leavePilot)
{
	if (leavePilot != 0) {
		buffer->dataSize = sizeof(struct paquetPilot);
		buffer = buffer->next;
	}

	while (buffer != NULL)
	{
		buffer->dataSize = 0;
		buffer = buffer->next;
	}
}

void resetCursor(struct buffer* buffer, int leavePilot)
{
	if (leavePilot != 0) {
		buffer->cursor = buffer->data + sizeof(struct paquetPilot);
		buffer = buffer->next;
	}

	while (buffer != NULL)
	{
		buffer->cursor = buffer->data;
		buffer = buffer->next;
	}
}

struct buffer *putData(struct buffer* buffer, char *sourceData, int sourceDataSize)
{
/*
	if (buffer->dataSize + sourceDataSize > buffer->bufferSize) {
		if (buffer->next == NULL) {
			buffer->next = peekBuffer(buffer->bufferSize);
			printf("PEEKED PEEKED PEEKED PEEKED PEEKED 1\n");
			if (buffer->next == NULL)
				return NULL;
		}

		buffer = buffer->next;
	}
*/
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
				buffer->next = peekBuffer(buffer->bufferSize);
				if (buffer->next == NULL)
					return NULL;
			}

			buffer = buffer->next;
		}
	}

	return buffer;
}

struct buffer *getData(struct buffer* buffer, char *destData, int destDataSize)
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

inline struct buffer *putUInt8(struct buffer* buffer, uint8_t sourceData)
{
	uint8_t value = sourceData;
	buffer = putData(buffer, (char *)&value, sizeof(value));
	return buffer;
}

inline struct buffer *putUInt32(struct buffer *buffer, uint32_t *sourceData)
{
	uint32_t value = htobe32(*sourceData);
	buffer = putData(buffer, (char *)&value, sizeof(value));
	return buffer;
}

inline struct buffer *getUInt32(struct buffer *buffer, uint32_t *destData)
{
	uint32_t value;
	buffer = getData(buffer, (char *)&value, sizeof(value));
	*destData = be32toh(value);
	return buffer;
}

inline struct buffer *putString(struct buffer* buffer, char *sourceData, int sourceDataSize)
{
	uint32_t swapped = htobe32(sourceDataSize);
	buffer = putData(buffer, (char *)&swapped, sizeof(swapped));
	if (sourceDataSize != 0)
		buffer = putData(buffer, sourceData, sourceDataSize);
	return buffer;
}

inline void booleanInternetToPostgres(char *value)
{
	*value = (*value == '0') ? 'f' : 't';
}

inline void booleanPostgresToInternet(char *value)
{
	*value = (*value == 'f') ? '0' : '1';
}

inline int isPostgresBooleanTrue(char *value)
{
	return (*value == 't');
}

inline int isPostgresBooleanFalse(char *value)
{
	return (*value == 'f');
}

int buffersInUse(int chainId)
{
	struct chain *chain = chains[chainId];
	pthread_spin_lock(&chain->lock);
	int buffersInUse = (chain->peekCursor >= chain->pokeCursor)
		? chain->peekCursor - chain->pokeCursor
		: chain->numberOfBuffers - (chain->pokeCursor - chain->peekCursor);
	pthread_spin_unlock(&chain->lock);
	return buffersInUse;
}
