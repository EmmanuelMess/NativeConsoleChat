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

static char username[MAX_SIZE_USERNAME] = {0};

void sendToServer(struct message message) {
	switch (message.messageType) {
		case USERNAME_MESSAGE: {
			// Se guarda el nombre de usuario del cliente
			strncpy(username, message.usernameMessage.username, MAX_SIZE_USERNAME);
			break;
		}
		case NAMED_TEXT_MESSAGE: {
			// Se guarda el nombre de usuario del cliente
			strncpy(message.namedTextMessage.username, username, MAX_SIZE_USERNAME);
			break;
		}
		default:
			//NOTHING
			break;
	}

	EXIT_ON_FALUIRE (send(sock_cli, &message, sizeof(struct message), 0));
}

void *sender(__attribute__((unused)) void *args) {
	pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, NULL);

	while (!killed) {
		struct unsent_message message = queueOutboundPeek();//Bloquea

		switch (message.message.messageType) {
			case NAMED_TEXT_MESSAGE:
			case USERNAME_MESSAGE:
			case PRIVATE_MESSAGE:
			case USER_EXITS_MESSAGE: {
				sendToServer(message.message);
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