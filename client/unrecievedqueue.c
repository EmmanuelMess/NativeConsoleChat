#include <pthread.h>
#include <malloc.h>
#include <stdatomic.h>
#include "unrecievedqueue.h"

TAILQ_HEAD(recieved_message_queue, recieved_message);
static struct recieved_message_queue history = {0};
static size_t queueSize = 0;
static pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t conditionNewAdded = PTHREAD_COND_INITIALIZER;
static atomic_bool newAndUnprocessed = false;

void queueInboundInit() {
	TAILQ_INIT(&history);
}

void queueInboundDestroy() {
	pthread_mutex_lock(&queueLock);
	while (!TAILQ_EMPTY(&history)) {
		struct recieved_message* e = TAILQ_FIRST(&history);
		TAILQ_REMOVE(&history, e, next);
		free(e);
	}
	pthread_mutex_unlock(&queueLock);
	pthread_mutex_destroy(&queueLock);
}
void queueInboundInsert(struct message messageFromServer) {
	struct recieved_message* message = malloc(sizeof(struct recieved_message));
	message->message = messageFromServer;

	pthread_mutex_lock(&queueLock);
	queueSize++;
	TAILQ_INSERT_TAIL(&history, message, next);
	newAndUnprocessed = true;
	pthread_cond_broadcast(&conditionNewAdded);
	pthread_mutex_unlock(&queueLock);
}

static struct recieved_message* getNth(size_t i) {
	if(i >= queueSize) {
		perror("Error: peek too much");
	}

	size_t pos = queueSize - (i + 1);//Da vuelta la indexacion
	struct recieved_message * item = TAILQ_LAST(&history, recieved_message_queue);

	for (size_t j = 0; j < pos; ++j) {
		item = TAILQ_PREV(item, recieved_message_queue, next);
	}
	return item;
}

struct message queueInboundPeek(size_t i) {
	pthread_mutex_lock(&queueLock);
	struct recieved_message * item = getNth(i);
	pthread_mutex_unlock(&queueLock);

	return item->message;
}

struct message queueInboundPeekOnAddBlocking() {
	pthread_mutex_lock(&queueLock);
	while(!newAndUnprocessed) {
		pthread_cond_wait(&conditionNewAdded, &queueLock);
	}
	newAndUnprocessed = false;
	struct recieved_message *message = TAILQ_LAST(&history, recieved_message_queue);
	pthread_mutex_unlock(&queueLock);

	return message->message;
}

size_t queueInboundSize() {
	pthread_mutex_lock(&queueLock);
	size_t r = queueSize;
	pthread_mutex_unlock(&queueLock);

	return r;
}