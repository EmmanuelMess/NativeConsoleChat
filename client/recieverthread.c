#include <sys/socket.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include "recieverthread.h"
#include "../messages.h"
#include "globalvars.h"
#include "unrecievedqueue.h"
#include "errormanagement.h"

void *reciever(__attribute__((unused)) void *args) {
	pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, NULL);

	while (!killed) {
		struct message recieved;

		ssize_t r = recv(sock_cli, &recieved, sizeof(struct message), 0);
		EXIT_ON_FALUIRE(r);
		if (r < (ssize_t) sizeof(struct message)) {
			continue;
		}

		switch (recieved.messageType){
			case NAMED_TEXT_MESSAGE: {
				recieved.namedTextMessage.username[MAX_SIZE_USERNAME - 1] = '\0';
				recieved.namedTextMessage.channel[MAX_SIZE_CHANNEL - 1] = '\0';
				recieved.namedTextMessage.data[MAX_SIZE_MESSAGE - 1] = '\0';
				queueInboundInsert(recieved);
				break;
			}
			case PRIVATE_MESSAGE: {
				recieved.privateMessage.sender[MAX_SIZE_USERNAME - 1] = '\0';
				recieved.privateMessage.receiver[MAX_SIZE_USERNAME - 1] = '\0';
				recieved.privateMessage.data[MAX_SIZE_MESSAGE - 1] = '\0';
				queueInboundInsert(recieved);
				break;
			}
			case USER_JOINS_MESSAGE: {
				recieved.userJoinsMessage.username[MAX_SIZE_USERNAME - 1] = '\0';
				recieved.userJoinsMessage.channel[MAX_SIZE_CHANNEL - 1] = '\0';
				queueInboundInsert(recieved);
				break;
			}
			case USER_EXITS_MESSAGE: {
				recieved.userExitsMessage.username[MAX_SIZE_USERNAME - 1] = '\0';
				recieved.userExitsMessage.channel[MAX_SIZE_CHANNEL - 1] = '\0';
				queueInboundInsert(recieved);
				break;
			}
			case RESPONSE_USERNAME_MESSAGE:
			case SERVER_TERMINATION_MESSAGE: {
				queueInboundInsert(recieved);
				break;
			}
			default: {
				perror("Mensaje mal formado");
				EXIT_ON_FALUIRE(-1);
			}

		}
	}

	return NULL;//Cancelled, see onExit
}