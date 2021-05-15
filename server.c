#include <netinet/in.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdatomic.h>
#include <sys/ioctl.h>
#include <string.h>
#include <signal.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <time.h>
#include "messages.h"
#define  EXIT_ON_FALUIRE(X)                  \
if ((X) == -1) {                             \
	perror("");                              \
	fflush(stderr);                          \
	onExit(-1);                              \
	exit(EXIT_FAILURE);                      \
}

void onExit(int signal);
static int socketServer;

int msleep(long msec) {
	struct timespec ts;
	int res;

	if (msec < 0) {
		errno = EINVAL;
		return -1;
	}

	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;

	do {
		res = nanosleep(&ts, &ts);
	} while (res && errno == EINTR);

	return res;
}

void createServer(int port) {
	struct sockaddr_in serverName = {0};

	socketServer = socket(AF_INET, SOCK_STREAM, 0);
	EXIT_ON_FALUIRE(socketServer);

	int value = 1;
	EXIT_ON_FALUIRE(setsockopt(socketServer, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)));

	serverName.sin_family = AF_INET;
	serverName.sin_addr.s_addr = inet_addr("0.0.0.0");
	serverName.sin_port = htons(port);

	while(bind(socketServer, (struct sockaddr *) &serverName, sizeof(struct sockaddr_in)) == -1) {
		printf("\rIntentando agarrarte el puerto ツ...");
	}
	printf("\n");
	EXIT_ON_FALUIRE(listen(socketServer, 5));
}

struct acceptedConnection {
	bool dead;
	int socket;
	char username[MAX_SIZE_USERNAME];
};

struct acceptedConnection acceptedConnections[25] = {0};
atomic_uint iRecieved = 0;//Powered by Apple (tm)
atomic_bool killed = false;

void sendFromServer(size_t i, char message[MAX_SIZE_MESSAGE]) {
	struct message messageFromServer = {0};
	messageFromServer.messageType = NAMED_TEXT_MESSAGE;
	strcpy(messageFromServer.namedTextMessage.username, "Server");
	strcpy(messageFromServer.namedTextMessage.data, message);
	EXIT_ON_FALUIRE(send(acceptedConnections[i].socket, &messageFromServer, sizeof(struct message), 0));
}

void* awaitConnections(__attribute__((unused)) void *args) {
	pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, NULL);

	while(!killed) {
		struct sockaddr_in clientName = {0};
		socklen_t size;
		int socketClient = accept(socketServer, (struct sockaddr *) &clientName, &size);

		acceptedConnections[iRecieved].socket = socketClient;

		iRecieved++;
	}

	return NULL;
}

pthread_t thread = {0};

void atExit() {
	onExit(-1);
}

void onExit(int signal) {
	if(killed) {
		return;
	}
	printf("\nExiting...");
	fflush(stdout);

	killed = true;

	if(thread != 0) {
		pthread_cancel(thread);
	}

	for (size_t i = 0; i < iRecieved; ++i) {
		shutdown(acceptedConnections[i].socket, SHUT_RDWR);
		close(acceptedConnections[i].socket);
	}

	shutdown(socketServer, SHUT_RDWR);
	close(socketServer);

	printf("Bye!\n");
	fflush(stdout);

	if(signal > 0) {
		exit(0);
	}
}

void sendDisconnection(size_t disconnectedUserIndex) {
	for (size_t i = 0; i < iRecieved; ++i) {
		if(acceptedConnections[i].dead) {
			continue;
		}

		// Verificacion de vividez
		char buffer[32];
		if (recv(acceptedConnections[i].socket, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0) {
			continue;
		}

		char text[MAX_SIZE_MESSAGE];
		sprintf(text, "--- Se desconecto: %s ---", acceptedConnections[disconnectedUserIndex].username);
		sendFromServer(i, text);
	}
}

bool checkIsAlive(size_t i) {
	if(acceptedConnections[i].dead) {
		return false;
	}

	char buffer[32];
	if (recv(acceptedConnections[i].socket, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0) {
		acceptedConnections[i].dead = true;
		printf("Usuario %s se desconecto\n", acceptedConnections[i].username);
		fflush(stdout);

		sendDisconnection(i);
		return false;
	}
	return true;
}

void recieveTextMessage(size_t i, struct text_message textMessage) {
	char *username = acceptedConnections[i].username;

	for (size_t j = 0; j < iRecieved; ++j) {
		if(!checkIsAlive(j)) {
			continue;
		}

		struct message msgResponse = {0};
		msgResponse.messageType = NAMED_TEXT_MESSAGE;
		strncpy(msgResponse.namedTextMessage.username, username, MAX_SIZE_USERNAME-1);
		strncpy(msgResponse.namedTextMessage.data, textMessage.data, MAX_SIZE_MESSAGE - 1);

		EXIT_ON_FALUIRE(send(acceptedConnections[j].socket, &msgResponse, sizeof(struct message), 0));
	}

	printf("[%s]> %s\n", username, textMessage.data);
}

void receivePrivateMessage(size_t i, struct message originalMessage) {
	if(originalMessage.messageType != PRIVATE_MESSAGE) {
		EXIT_ON_FALUIRE(-1);
	}
	struct private_message *message = &(originalMessage.privateMessage);
	strcpy(message->sender, acceptedConnections[i].username);

	for (size_t j = 0; j < iRecieved; ++j) {
		if(!checkIsAlive(j)) {
			continue;
		}

		if(strcmp(message->receiver, acceptedConnections[j].username) == 0) {
			EXIT_ON_FALUIRE(send(acceptedConnections[j].socket, &originalMessage, sizeof(struct message), 0));
			EXIT_ON_FALUIRE(send(acceptedConnections[i].socket, &originalMessage, sizeof(struct message), 0));
			return;
		}
	}

	sendFromServer(i, "Fallo al enviar el mensaje privado (usuario no encontrado)");
}

bool recieveUsername(size_t i, struct username_message message) {
	bool repeatedUsername = false;

	for (size_t j = 0; j < iRecieved; ++j) {
		if(strcmp(message.username, acceptedConnections[j].username) == 0 && checkIsAlive(j)) {
			repeatedUsername = true;
			break;
		}
	}

	struct message messageUsedName = {0};
	messageUsedName.messageType = RESPONSE_USERNAME_MESSAGE;
	messageUsedName.responseUsernameMessage.isFailure = repeatedUsername;
	EXIT_ON_FALUIRE(send(acceptedConnections[i].socket, &messageUsedName, sizeof(struct message), 0));

	if(repeatedUsername) {
		return false;
	}

	strncpy(acceptedConnections[i].username, message.username, MAX_SIZE_USERNAME-1);

	char welcomeText[MAX_SIZE_MESSAGE];
	sprintf(welcomeText, "--- Ha aparecido: %s ---", message.username);

	for (size_t j = 0; j < i; ++j) {
		if(!checkIsAlive(j)) {
			continue;
		}

		sendFromServer(j, welcomeText);
	}

	printf("--- Ha aparecido: %s ---\n", message.username);
	fflush(stdout);
	return true;
}

int main(__attribute__((unused)) int argc, char *argv[]) {
	printf("Starting...");

	atexit(atExit);
	signal(SIGTERM, onExit);
	signal(SIGKILL, onExit);
	signal(SIGPIPE, onExit);

	createServer(atoi(argv[1]));

	pthread_create(&thread, NULL, awaitConnections, NULL);

	printf("Started\n");
	while(true) {
		msleep(iRecieved > 2? 1000/iRecieved : 500);

		for (size_t i = 0; i < iRecieved; ++i) {
			int socketClient = acceptedConnections[i].socket;

			int count;
			ioctl(socketClient, FIONREAD, &count);

			if (count != (ssize_t) sizeof(struct message)) {
				continue;
			}

			struct message msg = {0};
			ssize_t r;
			r = recv(socketClient, &msg, sizeof(struct message), MSG_DONTWAIT);

			if(r == EAGAIN) {//TODO esto usa todo el core
				continue;//Si no recibi nada sigo
			}

			switch (msg.messageType){
				case TEXT_MESSAGE: {
					msg.textMessage.data[MAX_SIZE_MESSAGE - 1] = '\0';
					recieveTextMessage(i, msg.textMessage);
					break;
				}
				case USERNAME_MESSAGE: {
					msg.usernameMessage.username[MAX_SIZE_USERNAME - 1] = '\0';
					if(!recieveUsername(i, msg.usernameMessage)) {
						continue;
					}
					break;
				}
				case PRIVATE_MESSAGE: {
					msg.privateMessage.receiver[MAX_SIZE_USERNAME - 1] = '\0';
					msg.privateMessage.data[MAX_SIZE_MESSAGE - 1] = '\0';
					receivePrivateMessage(i, msg);
					break;
				}
				case USER_EXITS_MESSAGE: {
					sendDisconnection(i);
					break;
				}
				default: {
					printf("Mensaje malformado (tipo: %d, tamaño: %d)\n", msg.messageType, count);
					fflush(stdout);
					continue;
				}
			}
		}
	}
}