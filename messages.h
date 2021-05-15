#ifndef COMUNICARPCS_MESSAGES_H
#define COMUNICARPCS_MESSAGES_H

#include <stdbool.h>

/*
 * this must be pragma packed so that all compilations maintain
 * consistent structures for receiving and reading the values to
 * the correct positions, with bit precision.
 */
#pragma pack(push, 8)
//TODO the pragma packed, it can be very inefficient

#define MAX_SIZE_USERNAME 35
struct username_message {
	char username[MAX_SIZE_USERNAME];
};

#define MAX_SIZE_MESSAGE 1024
struct text_message {
	char data[MAX_SIZE_MESSAGE];
};

struct named_text_message {
	char username[MAX_SIZE_USERNAME];
	char data[MAX_SIZE_MESSAGE];
};

struct private_message {
	char sender[MAX_SIZE_USERNAME];
	char receiver[MAX_SIZE_USERNAME];
	char data[MAX_SIZE_MESSAGE];
};

struct response_username_message {
	bool isFailure;
};

struct user_exits_message {};

/*
 * Invariantes:
 *  - Toda campo de tipo enum message_type se chequea con un switch o con un if
 *  - Siempre que se switch o if sobre un campo de este tipo, se debe incluir el caso UNSET (o default), que tira un error
 */
enum message_type { UNSET = 0, USERNAME_MESSAGE = 1, TEXT_MESSAGE = 2, NAMED_TEXT_MESSAGE = 3,
	RESPONSE_USERNAME_MESSAGE = 4, PRIVATE_MESSAGE = 5, USER_EXITS_MESSAGE = 6 };

struct message {
	union {
		struct username_message usernameMessage;
		struct text_message textMessage;
		struct named_text_message namedTextMessage;
		struct response_username_message responseUsernameMessage;
		struct private_message privateMessage;
		struct user_exits_message userExitsMessage;
	};
	enum message_type messageType;
};

#pragma pack(pop)

#endif //COMUNICARPCS_MESSAGES_H
