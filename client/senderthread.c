#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include "senderthread.h"
#include "../messages.h"
#include "unsentqueue.h"
#include "globalvars.h"
#include "unrecievedqueue.h"
#include "errormanagement.h"

void *sender(__attribute__((unused)) void *args) {
	pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, NULL);

	while (!killed) {
		struct unsent_message message = queueOutboundPeek();//Bloquea

		switch (message.message.messageType) {
			case NAMED_TEXT_MESSAGE:
			case USERNAME_MESSAGE:
			case PRIVATE_MESSAGE:
			case USER_JOINS_MESSAGE:
			case USER_EXITS_MESSAGE:{
				EXIT_ON_FALUIRE (send(sock_cli, &(message.message), sizeof(struct message), 0));
				break;
			}
			default: {
				perror("Mensaje saliente mal formado");
				EXIT_ON_FALUIRE(-1);
			}
		}

		queueOutboundRemove();
	}

	return NULL;//Canceled see onExit(int)
}