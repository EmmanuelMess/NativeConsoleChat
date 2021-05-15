#ifndef COMUNICARPCS_ERRORMANAGEMENT_HPP
#define COMUNICARPCS_ERRORMANAGEMENT_HPP
#include <stdio.h>
#include <stdlib.h>

#define  EXIT_ON_FALUIRE(X)                  \
if ((X) == -1) {                             \
	perror("");                              \
	onExit(-1);                               \
	exit(EXIT_FAILURE);                      \
}

void onExit(int signal);

#endif //COMUNICARPCS_ERRORMANAGEMENT_HPP
