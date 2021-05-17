#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "compatinet.h"
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

void createServer(int port) {
	struct sockaddr_in serverName = {0};

	socketServer = socket(AF_INET, SOCK_STREAM, 0);
	EXIT_ON_FALUIRE(socketServer);

#ifdef _WIN32
	u_long mode = 1;  // 1 to enable non-blocking socket
	ioctlsocket(socketServer, FIONBIO, &mode);
#endif

	int value = 1;
	EXIT_ON_FALUIRE(setsockopt(socketServer, SOL_SOCKET, SO_REUSEADDR, CAST_TO_SEND_BUFFER(&value), sizeof(int)));

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

struct acceptedConnection acceptedConnections[25] = {0};//TODO linked list
atomic_uint iRecieved = 0;//Powered by Apple (tm)
atomic_bool killed = false;

void sendFromServer(size_t i, char message[MAX_SIZE_MESSAGE]) {
	struct message messageFromServer = {0};
	messageFromServer.messageType = NAMED_TEXT_MESSAGE;
	strcpy(messageFromServer.namedTextMessage.username, "Server");
	strcpy(messageFromServer.namedTextMessage.channel, "General");
	strcpy(messageFromServer.namedTextMessage.data, message);

	EXIT_ON_FALUIRE(send(acceptedConnections[i].socket, CAST_TO_SEND_BUFFER(&messageFromServer), sizeof(struct message), 0));
}

void* awaitConnections(__attribute__((unused)) void *args) {
	pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, NULL);

	while(!killed) {
		struct sockaddr_in clientName = {0};

		ACCEPT_LEN_TYPE size;
		int socketClient = accept(socketServer, (struct sockaddr *) &clientName, &size);
#ifdef _WIN32
		u_long mode = 1;  // 1 to enable non-blocking socket
		ioctlsocket(socketClient, FIONBIO, &mode);
#endif
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
		shutdown(acceptedConnections[i].socket, SHUTDOWN_ALL);
		close(acceptedConnections[i].socket);
	}

	shutdown(socketServer, SHUTDOWN_ALL);
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
		if (recv(acceptedConnections[i].socket, buffer, sizeof(buffer), ADD_FLAG_NONBLOCKING(MSG_PEEK)) == 0) {
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
	if (recv(acceptedConnections[i].socket, buffer, sizeof(buffer), ADD_FLAG_NONBLOCKING(MSG_PEEK)) == 0) {
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
		strncpy(msgResponse.namedTextMessage.channel, textMessage.channel, MAX_SIZE_CHANNEL);
		strncpy(msgResponse.namedTextMessage.data, textMessage.data, MAX_SIZE_MESSAGE);
		EXIT_ON_FALUIRE(send(acceptedConnections[j].socket, CAST_TO_SEND_BUFFER(&msgResponse), sizeof(struct message), 0));
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
			EXIT_ON_FALUIRE(send(acceptedConnections[j].socket, CAST_TO_SEND_BUFFER(&originalMessage), sizeof(struct message), 0));
			EXIT_ON_FALUIRE(send(acceptedConnections[i].socket, CAST_TO_SEND_BUFFER(&originalMessage), sizeof(struct message), 0));
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

	EXIT_ON_FALUIRE(send(acceptedConnections[i].socket, CAST_TO_SEND_BUFFER(&messageUsedName), sizeof(struct message), 0));

	if(repeatedUsername) {
		return false;
	}

	strncpy(acceptedConnections[i].username, message.username, MAX_SIZE_USERNAME);

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

ssize_t pollAllSocketsForMessages() {
	fd_set readfds;
	FD_ZERO(&readfds);

	int maxfd = -1;
	for (size_t i = 0; i < iRecieved; i++) {
		if(!checkIsAlive(i)) {
			continue;
		}
		int socket = acceptedConnections[i].socket;

		FD_SET(socket, &readfds);
		if (socket > maxfd) {
			maxfd = socket;
		}
	}

	struct timeval t = (struct timeval) { .tv_usec = 250*1000, .tv_sec = 0};
	int status = select(maxfd + 1, &readfds, NULL, NULL, &t);
	if (status <= 0) {
		return -1;
	}

	ssize_t fdindex = -1;
	for (size_t i = 0; i < iRecieved; i++) {
		if(!checkIsAlive(i)) {
			continue;
		}
		int socket = acceptedConnections[i].socket;

		if (FD_ISSET(socket, &readfds)) {
			fdindex = (ssize_t) i;
			break;
		}
	}

	return fdindex;
}

int main(__attribute__((unused)) int argc, char *argv[]) {
	if(argc < 2) {
		printf("Uso: server <puerto>\n");
		return 0;
	}

	printf("Starting...");

	atexit(atExit);

#ifdef _WIN32
	signal(SIGTERM, onExit);
	signal(SIGABRT, onExit);
#else
	signal(SIGTERM, onExit);
	signal(SIGKILL, onExit);
	signal(SIGPIPE, onExit);
#endif

	createServer(atoi(argv[1]));

	pthread_create(&thread, NULL, awaitConnections, NULL);

	printf("Started\n");
	while(true) {
		ssize_t i = pollAllSocketsForMessages();
		if (i == -1) {
			continue;
		}

		int socketClient = acceptedConnections[i].socket;

#ifdef _WIN32
#else
		int count;
		ioctl(socketClient, FIONREAD, &count);

		if (count != (ssize_t) sizeof(struct message)) {
			continue;
		}
#endif
		struct message msg = {0};

		ssize_t r;
		r = recv(socketClient, CAST_TO_RECV_BUFFER(&msg), sizeof(struct message), ADD_FLAG_NONBLOCKING(0));

#ifdef _WIN32
		if(r == WSAEWOULDBLOCK) {
			continue;//Si no recibi nada sigo
		}
		if(r != (ssize_t) sizeof(struct message)) {
			continue;
		}
#else
		if (r == EAGAIN) {
			continue;//Si no recibi nada sigo
		}
#endif

		switch (msg.messageType) {
			case TEXT_MESSAGE: {
				msg.textMessage.channel[MAX_SIZE_CHANNEL - 1] = '\0';
				msg.textMessage.data[MAX_SIZE_MESSAGE - 1] = '\0';
				recieveTextMessage(i, msg.textMessage);
				break;
			}
			case USERNAME_MESSAGE: {
				msg.usernameMessage.username[MAX_SIZE_USERNAME - 1] = '\0';
				if (!recieveUsername(i, msg.usernameMessage)) {
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
				printf("Mensaje malformado (tipo: %d)\n", msg.messageType);
				fflush(stdout);
				continue;
			}
		}
	}
}