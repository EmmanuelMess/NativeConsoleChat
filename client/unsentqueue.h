#ifndef COMUNICARPCS_UNSENTQUEUE_H
#define COMUNICARPCS_UNSENTQUEUE_H

#include <sys/queue.h>
#include <stdbool.h>
#include "../messages.h"

struct unsent_message {
	struct message message;
	TAILQ_ENTRY(unsent_message) next;
};


void queueOutboundInit();
void queueOutboundDestroy();
void queueOutboundInsert(struct message outboundMessage);
/**
 * Blocking
 */
struct unsent_message queueOutboundPeek();
void queueOutboundRemove();

#endif //COMUNICARPCS_UNSENTQUEUE_H
