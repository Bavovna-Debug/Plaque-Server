#ifndef _BUFFERS_
#define _BUFFERS_

#include <netinet/in.h>

#define	KB 1024
#define MB 1024 * 1024

#define BUFFER1K				0
#define BUFFER4K				1
#define BUFFER1M				2

typedef struct buffer {
	int					bufferId;
	int					chainId;
	struct buffer		*next;
	int					bufferSize;
	int					dataSize;
	char				*data;
	char				*cursor;
} buffer_t;

void constructBuffers(void);

void destructBuffers(void);

struct buffer* peekBuffer(int preferredSize);

void pokeBuffer(struct buffer* buffer);

struct buffer* cloneBuffer(struct buffer* buffer);

int totalDataSize(struct buffer* buffer);

void resetBufferData(struct buffer* buffer, int leavePilot);

void resetCursor(struct buffer* buffer, int leavePilot);

struct buffer *putData(struct buffer* buffer, char *sourceData, int sourceDataSize);

struct buffer *getData(struct buffer* buffer, char *destData, int destDataSize);

inline struct buffer *putUInt8(struct buffer* buffer, int sourceData);

inline struct buffer *putUInt32(struct buffer* buffer, uint32_t *sourceData);

inline struct buffer *getUInt32(struct buffer* buffer, uint32_t *destData);

inline struct buffer *putString(struct buffer* buffer, char *sourceData, int sourceDataSize);

inline void convertBooleanToPostgres(char *value);

inline void convertBooleanToInternet(char *value);

int buffersInUse(int chainId);

#endif
