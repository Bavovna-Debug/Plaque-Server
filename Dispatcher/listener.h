#include <netinet/in.h>

typedef struct listenerArguments {
	uint16_t portNumber;
} listenerArguments;

void
constructDialogues(void);

void
destructDialogues(void);

void *
dialogueThread(void *arg);

void
dialogueInit(int sockFD, struct in_addr *satelliteAddress);

void *
listenerThread(void *arg);
