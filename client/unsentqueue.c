#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include "unsentqueue.h"

TAILQ_HEAD(unsent_messages_queue, unsent_message);// Se declara unsents del tipo tailHead
static struct unsent_messages_queue unsents = {0};
static pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t conditionNotEmpty = PTHREAD_COND_INITIALIZER;

void queueOutboundInit() {
	TAILQ_INIT(&unsents);
}

void queueOutboundDestroy() {
	pthread_mutex_lock(&queueLock);
	while (!TAILQ_EMPTY(&unsents)) {
		struct unsent_message* e = TAILQ_FIRST(&unsents);
		TAILQ_REMOVE(&unsents, e, next);
		free(e);
	}
	pthread_mutex_unlock(&queueLock);
	pthread_mutex_destroy(&queueLock);
}

void queueOutboundInsert(struct message outboundMessage) {
	struct unsent_message* message = malloc(sizeof(struct unsent_message));
	memcpy(&(message->message), &outboundMessage, sizeof (struct message));

	pthread_mutex_lock(&queueLock);
	TAILQ_INSERT_TAIL(&unsents, message, next);
	pthread_cond_broadcast(&conditionNotEmpty);
	pthread_mutex_unlock(&queueLock);
}

struct unsent_message queueOutboundPeek() {
	pthread_mutex_lock(&queueLock);
	while(TAILQ_EMPTY(&unsents)) {
		pthread_cond_wait(&conditionNotEmpty, &queueLock);
	}
	struct unsent_message *message = TAILQ_FIRST(&unsents);
	pthread_mutex_unlock(&queueLock);

	return *message;
}

void queueOutboundRemove() {
	pthread_mutex_lock(&queueLock);
	struct unsent_message *message = TAILQ_FIRST(&unsents);
	TAILQ_REMOVE(&unsents, message, next);
	pthread_mutex_unlock(&queueLock);
	free(message);
}