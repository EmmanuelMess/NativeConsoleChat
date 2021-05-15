#ifndef COMUNICARPCS_UNRECIEVEDQUEUE_H
#define COMUNICARPCS_UNRECIEVEDQUEUE_H

#include <sys/queue.h>
#include "../messages.h"
#include <stddef.h>

/**
 * Invariantes:
 * Nunca se eliminan mensajes imprimibles
 * Siempre se agrega al final
 */
struct recieved_message {
	struct message message;
	TAILQ_ENTRY(recieved_message) next;
};

void queueInboundInit();

void queueInboundDestroy();

void queueInboundInsert(struct message messageFromServer);

struct message queueInboundPeek(size_t i);

struct message queueInboundPeekOnAddBlocking();

size_t queueInboundSize();

#endif //COMUNICARPCS_UNRECIEVEDQUEUE_H
