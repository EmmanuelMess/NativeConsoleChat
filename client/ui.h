#ifndef COMUNICARPCS_UI_H
#define COMUNICARPCS_UI_H

#include <ncurses.h>

WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);

_Noreturn void ui();
void killUi();

#endif //COMUNICARPCS_UI_H
