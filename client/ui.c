#include <string.h>
#include <ctype.h>
#include <locale.h>
#include "ui.h"
#include "../messages.h"
#include "unsentqueue.h"
#include "unrecievedqueue.h"
#include "errormanagement.h"

bool isPrintable(struct message message) {
	return message.messageType == NAMED_TEXT_MESSAGE || message.messageType == PRIVATE_MESSAGE;
}

bool isAcceptedChar(int c) {
	return !iscntrl(c);
}

#define EXIT_COMMAND "/exit"
#define PM_COMMAND "/msg "
#define CHANGE_USERNAME_COMMAND "/nickname"

void queueOutboundMessage(char message[MAX_SIZE_MESSAGE]) {
	struct message messageToServer = {0};

	if(strcmp(message, EXIT_COMMAND) == 0) {
		messageToServer.messageType = USER_EXITS_MESSAGE;
		exit(0);
	}

	bool hasAtLeastTwoSpaces = strchr(message, ' ') != NULL && strchr(message, ' ') != strrchr(message, ' ');

	//CHANGE USERNAME COMMAND IS IN UI!

	if(strncmp(message, PM_COMMAND, strlen(PM_COMMAND)) == 0 && hasAtLeastTwoSpaces) {
		message += strlen(PM_COMMAND);
		char *receiver = message;

		message = strchr(message, ' ');
		*message = '\0';
		message++;

		messageToServer.messageType = PRIVATE_MESSAGE;
		strncpy(messageToServer.privateMessage.receiver, receiver, MAX_SIZE_USERNAME - 1);
		strncpy(messageToServer.privateMessage.data, message, MAX_SIZE_MESSAGE - 1);
	} else {
		messageToServer.messageType = TEXT_MESSAGE;
		strncpy(messageToServer.textMessage.data, message, MAX_SIZE_MESSAGE - 1);
	}

	queueOutboundInsert(messageToServer);
}

void writeMessageToWindow(WINDOW* w, int y, int x, struct message message) {
	if(!isPrintable(message)) {
		perror("Mensaje no printeable");
		EXIT_ON_FALUIRE(-1);
	}

	switch (message.messageType) {
		case NAMED_TEXT_MESSAGE:
			mvwprintw(w, y, x, "[%s]> %s", message.namedTextMessage.username, message.namedTextMessage.data);
			break;
		case PRIVATE_MESSAGE:
			mvwprintw(w, y, x, "[MP][%s->%s]> %s", message.privateMessage.sender, message.privateMessage.receiver, message.privateMessage.data);
			break;
		default:
			EXIT_ON_FALUIRE(-1);
	}
}

_Noreturn void ui() {
	initscr();// Start curses mode

	int maxY, maxX;
	getmaxyx(stdscr, maxY, maxX);

	readName: {
		size_t positionWhenCreatingName = queueInboundSize();

		curs_set(1);
		WINDOW *windowNombre = newwin(3, MAX_SIZE_USERNAME + 2, maxY / 2 - 1, (MAX_SIZE_USERNAME + 2) / 2 - 1);
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
		char message[500];
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

				if(strncmp(message, CHANGE_USERNAME_COMMAND, strlen(CHANGE_USERNAME_COMMAND)) == 0) {
					delwin(windowMessages);
					delwin(windowText);
					refresh();
					goto readName;
				}

				queueOutboundMessage(message);
				messageIterator = 0;

				for (int x = 1; x < maxX-1; ++x) {
					mvwaddch(windowText, 1, x, ' ');
				}
				wmove(windowText, 1, 1);
				wrefresh(windowText);
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

			if (ch == KEY_DOWN && freeze) {
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