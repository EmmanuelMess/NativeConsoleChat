#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "ui.h"
#include "senderthread.h"
#include "recieverthread.h"
#include "globalvars.h"
#include "unsentqueue.h"
#include "unrecievedqueue.h"
#include "errormanagement.h"

atomic_bool killed = false;
int sock_cli;

void connectServer(char *ip, int port) {
	sock_cli = socket(AF_INET, SOCK_STREAM, 0);
	EXIT_ON_FALUIRE(sock_cli);

	struct sockaddr_in srv_nombre = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = inet_addr(ip),
		.sin_port = htons(port)
	};

	EXIT_ON_FALUIRE(connect(sock_cli, (struct sockaddr *) &srv_nombre, sizeof(struct sockaddr_in)));
}

pthread_t recieverThread;
pthread_t senderThread;

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

	queueOutboundDestroy();
	queueInboundDestroy();

	if(recieverThread != 0) {
		pthread_cancel(recieverThread);
	}

	if(senderThread != 0) {
		pthread_cancel(senderThread);
	}

	shutdown(sock_cli, SHUT_RDWR);
	close(sock_cli);

	killUi();

	printf("Bye!\n");
	fflush(stdout);

	if(signal > 0) {
		exit(0);
	}
}

/**
 * Para que la ui sea responsiva se requiere que los locks nunca
 * hagan comunicacion con el servidor.
 */
int main(__attribute__((unused)) int argc, char *argv[]) {
	atexit(atExit);
	signal(SIGTERM, onExit);
	signal(SIGKILL, onExit);
	signal(SIGPIPE, onExit);

	queueOutboundInit();
	queueInboundInit();

	printf("Conectando a %s:%d", argv[1], atoi(argv[2]));

	connectServer(argv[1], atoi(argv[2]));

	pthread_create(&recieverThread, NULL, reciever, NULL);
	pthread_create(&senderThread, NULL, sender, NULL);

	ui();//blocking
}