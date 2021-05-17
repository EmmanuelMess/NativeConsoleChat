#include <string.h>
#include <ctype.h>
#include "ui.h"
#include "../messages.h"
#include "unsentqueue.h"
#include "unrecievedqueue.h"
#include "errormanagement.h"

char currentChannel[MAX_SIZE_CHANNEL] = "General";

bool isPrintable(struct message message) {
	return (message.messageType == NAMED_TEXT_MESSAGE && strcmp(message.namedTextMessage.channel, currentChannel) == 0)
			|| message.messageType == PRIVATE_MESSAGE || message.messageType == SERVER_TERMINATION_MESSAGE;
}

bool isAcceptedChar(int c) {
	return 0 <= c && c <= 255 && !iscntrl(c);
}

#define EXIT_COMMAND_STRING "/exit"
#define PM_COMMAND_STRING "/msg "
#define CHANGE_USERNAME_COMMAND_STRING "/nickname"
#define JOIN_STRING "/join "
#define CHANNEL_STRING "/channel"

enum command { EXIT, PM, CHANGE_USERNAME, JOIN, CHANNEL };

bool checkIsCommand(char *message, enum command command) {
	switch (command) {
		case EXIT: {
			return strcmp(message, EXIT_COMMAND_STRING) == 0;
		}
		case PM: {
			bool hasAtLeastTwoSpaces = strchr(message, ' ') != NULL && strchr(message, ' ') != strrchr(message, ' ');
			return strncmp(message, PM_COMMAND_STRING, strlen(PM_COMMAND_STRING)) == 0 && hasAtLeastTwoSpaces;
		}
		case CHANGE_USERNAME: {
			return strncmp(message, CHANGE_USERNAME_COMMAND_STRING, strlen(CHANGE_USERNAME_COMMAND_STRING)) == 0;
		}
		case JOIN: {
			return strncmp(message, JOIN_STRING, strlen(JOIN_STRING)) == 0;
		}
		case CHANNEL: {
			return strncmp(message, CHANNEL_STRING, strlen(CHANNEL_STRING)) == 0;
		}
		default: {
			perror("Unhandled command");
			EXIT_ON_FALUIRE(-1);
		}
	}
}

void writeMessageToWindow(WINDOW* w, int y, int x, struct message message) {
	if(!isPrintable(message)) {
		perror("Mensaje no printeable");
		EXIT_ON_FALUIRE(-1);
	}

	switch (message.messageType) {
		case NAMED_TEXT_MESSAGE:
			mvwprintw(w, y, x, "[%s|%s]> %s", message.namedTextMessage.channel, message.namedTextMessage.username, message.namedTextMessage.data);
			break;
		case PRIVATE_MESSAGE:
			mvwprintw(w, y, x, "[MP][%s->%s]> %s", message.privateMessage.sender, message.privateMessage.receiver, message.privateMessage.data);
			break;
		case SERVER_TERMINATION_MESSAGE:
			mvwprintw(w, y, x, "El servidor murio, no queda esperanza alguna");
			break;
		default:
			EXIT_ON_FALUIRE(-1);
	}
}

/**
 * Esto esta programado como cambios a una consola,
 * no como una serie de operaciones que construyen
 * sobre las operaciones anteriores
 */
_Noreturn void ui() {
	initscr();// Start curses mode

	int maxY, maxX;
	getmaxyx(stdscr, maxY, maxX);

	readName: {
		size_t positionWhenCreatingName = queueInboundSize();

		curs_set(1);
		WINDOW *windowNombre = newwin(3, MAX_SIZE_USERNAME + 2, maxY / 2 - 1,  (maxX)/2 - (MAX_SIZE_USERNAME + 2)/2 - 1);
		box(windowNombre, 0, 0);        // 0, 0 gives default characters for the vertical and horizontal  lines
		mvwprintw(windowNombre, 0, 2, "Nombre");
		wrefresh(windowNombre);


		struct message usernameMessage = {0};
		usernameMessage.messageType = USERNAME_MESSAGE;
		mvwscanw(windowNombre, 1, 1, "%[^\n]", usernameMessage.usernameMessage.username);//TODO empty
		queueOutboundInsert(usernameMessage); // Al conectarme mando el username

		struct message inboundMessage;
		for (; true; ++positionWhenCreatingName) {
			while(positionWhenCreatingName >= queueInboundSize()) {
				queueInboundPeekOnAddBlocking();
			}
			inboundMessage = queueInboundPeek(positionWhenCreatingName);

			if(inboundMessage.messageType == UNSET) {
				perror("Mensaje mal formado");
				EXIT_ON_FALUIRE(-1);
			}

			if(inboundMessage.messageType == RESPONSE_USERNAME_MESSAGE) {
				if(!inboundMessage.responseUsernameMessage.isFailure) {
					delwin(windowNombre);
					goto executeInitChatWindow;
				}

				mvwprintw(windowNombre, 1, 1, "(Nombre repetido)");
				wgetch(windowNombre);

				delwin(windowNombre);
				goto readName;
			}
		}
	}

	executeInitChatWindow: {
		cbreak();// Line buffering disabled, Pass on everty thing to me

		const int printableCuantity = maxY - 3;

		WINDOW *windowMessages = newwin(maxY - 3, maxX, 0, 0);
		scrollok(windowMessages, true);
		idlok(windowMessages, true);

		WINDOW *windowText = newwin(3, maxX, maxY - 3, 0);
		box(windowText, 0, 0);        // 0, 0 gives default characters for the vertical and horizontal  lines
		keypad(windowText, TRUE);
		wtimeout(windowText, 5);

		wrefresh(windowMessages);
		wrefresh(windowText);
		wmove(windowText, 1, 1);

		const int messageTextMargin = 2;

		size_t nextToPrint = 0; // Invariante el indice es el siguiente al ultimo impreso en pantalla
		bool freeze = false;
		char message[MAX_SIZE_MESSAGE];
		int messageIterator = 0;

		while (true) {
			if (nextToPrint < queueInboundSize() && !freeze) {
				struct message peekMessage = queueInboundPeek(nextToPrint);
				if(!isPrintable(peekMessage)) {
					nextToPrint++;
					continue;
				}

				curs_set(0);
				wscrl(windowMessages, 1);

				writeMessageToWindow(windowMessages, printableCuantity - 1, 1, peekMessage);

				wmove(windowText, 1, 1 + messageIterator);
				curs_set(1);
				wrefresh(windowMessages);

				nextToPrint++;
			}

			int ch = wgetch(windowText);
			if (ch == ERR) continue;

			if (ch == KEY_BACKSPACE) {
				if(messageIterator -1 >= (maxX - 2 - messageTextMargin)) {
					messageIterator--;

					int updatedCursorPos = maxX - 1 - messageTextMargin;
					message[messageIterator] = '\0';

					mvwprintw(windowText, 1, 1, &(message[messageIterator - (maxX - 2 - messageTextMargin)]));
					mvwprintw(windowText, 1, updatedCursorPos, " ");
					wmove(windowText, 1, updatedCursorPos);
					wrefresh(windowText);
					continue;
				}

				if (messageIterator > 0) {
					mvwprintw(windowText, 1, messageIterator, " ");
					wmove(windowText, 1, messageIterator);
					wrefresh(windowText);

					messageIterator--;
					continue;
				}

				wmove(windowText, 1, 1 + messageIterator);
			}

			if (ch == '\n') {
				if (messageIterator == 0) {
					wmove(windowText, 1, 1 + messageIterator);
					continue;
				}

				message[messageIterator] = '\0';

				{
					if (checkIsCommand(message, CHANGE_USERNAME)) {
						delwin(windowMessages);
						delwin(windowText);
						refresh();
						goto readName;
					}

					if(checkIsCommand(message, EXIT)) {
						queueOutboundInsert((struct message) { .messageType = USER_EXITS_MESSAGE });//Hope this reaches the server
						exit(0);
					}

					if(checkIsCommand(message, JOIN)) {
						char *channel = message + strlen(JOIN_STRING);

						strncpy(currentChannel, channel, MAX_SIZE_CHANNEL-1);

						struct message ficticiousChannelMessage = (struct message) {
							.messageType = NAMED_TEXT_MESSAGE,
							.namedTextMessage.username = "Server"
						};

						strncpy(ficticiousChannelMessage.namedTextMessage.channel, currentChannel, MAX_SIZE_CHANNEL);
						sprintf(ficticiousChannelMessage.namedTextMessage.data, "Te uniste al canal: %s", currentChannel);

						queueInboundInsert(ficticiousChannelMessage);

						wscrl(windowMessages, printableCuantity);

						nextToPrint = 0;
						goto endParse;
					}

					struct message messageToServer = {0};
					char *sentMessage = message;

					if(checkIsCommand(sentMessage, PM)) {
						sentMessage += strlen(PM_COMMAND_STRING);
						char *receiver = sentMessage;

						sentMessage = strchr(sentMessage, ' ');
						*sentMessage = '\0';
						sentMessage++;

						messageToServer.messageType = PRIVATE_MESSAGE;
						strncpy(messageToServer.privateMessage.receiver, receiver, MAX_SIZE_USERNAME - 1);
						strncpy(messageToServer.privateMessage.data, sentMessage, MAX_SIZE_MESSAGE - 1);
					} else {
						messageToServer.messageType = NAMED_TEXT_MESSAGE;
						strncpy(messageToServer.namedTextMessage.channel, currentChannel, MAX_SIZE_CHANNEL);
						strncpy(messageToServer.namedTextMessage.data, sentMessage, MAX_SIZE_MESSAGE);
					}

					queueOutboundInsert(messageToServer);
				}

				endParse: {
					messageIterator = 0;

					for (int x = 1; x < maxX - 1; ++x) {
						mvwaddch(windowText, 1, x, ' ');
					}
					wmove(windowText, 1, 1);
					wrefresh(windowText);
				}
				continue;
			}

			if (ch == KEY_UP) {
				if(nextToPrint == 0) {
					continue;
				}

				ssize_t historicalPritableIndex = ((ssize_t) nextToPrint) - 1;//Asi esta sobre el ultimo printeado en pantalla
				historicalPritableIndex -= printableCuantity;

				for(; historicalPritableIndex >= 0; historicalPritableIndex--) {
					struct message recievedMessage = queueInboundPeek(historicalPritableIndex);
					if(isPrintable(recievedMessage)) {
						break;
					}
				}

				if (historicalPritableIndex >= 0) {
					//Hay al menos printableCuantity elementos en pantalla
					freeze = true;
					do {
						nextToPrint--;
					} while(!isPrintable(queueInboundPeek(nextToPrint-1)));
					wscrl(windowMessages, -1);

					struct message recievedMessage = queueInboundPeek(historicalPritableIndex);

					writeMessageToWindow(windowMessages, 0, 1, recievedMessage);
					wrefresh(windowMessages);
				}

				continue;
			}

			if (ch == KEY_DOWN) {
				if(!freeze) {
					continue;
				}

				if(nextToPrint == queueInboundSize()) {
					freeze = false;
					continue;
				}

				size_t futurePrintableIndex = nextToPrint;

				for(; futurePrintableIndex < queueInboundSize(); futurePrintableIndex++) {
					struct message recievedMessage = queueInboundPeek(futurePrintableIndex);
					if(isPrintable(recievedMessage)) {
						break;
					}
				}

				if (futurePrintableIndex < queueInboundSize()) {
					wscrl(windowMessages, 1);

					nextToPrint = futurePrintableIndex+1;

					struct message recievedMessage = queueInboundPeek(futurePrintableIndex);
					writeMessageToWindow(windowMessages, printableCuantity - 1, 1, recievedMessage);
				}

				wrefresh(windowMessages);
				continue;
			}

			if (messageIterator + 1 < MAX_SIZE_MESSAGE && isAcceptedChar(ch)) {
				message[messageIterator++] = (char) ch;

				if(messageIterator >= (maxX - 2 - messageTextMargin)) {
					int updatedCursorPos = maxX - 1 - messageTextMargin;
					message[messageIterator] = '\0';

					mvwprintw(windowText, 1, 1, &(message[messageIterator - (maxX - 2 - messageTextMargin)]));
					mvwprintw(windowText, 1, updatedCursorPos, " ");
					wmove(windowText, 1, updatedCursorPos);
					wrefresh(windowText);
				}

				continue;
			}

			mvwaddch(windowText, 1, messageIterator + 1, ' ');
			wmove(windowText, 1, messageIterator+1);
		}
	}
}

void killUi() {
	endwin();			/* End curses mode		  */
}