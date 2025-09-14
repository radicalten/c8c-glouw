#ifndef CHIP8_TUI
#define CHIP8_TUI

/*
    This is a TUI (Terminal UI) frontend for the chip.h interpreter. This requires ncurses or another ncurses compatible library.
*/

#include <stdlib.h>
#include <ncurses.h>
#include "chip.h"

void renderer_init();
void renderer_refresh(C8State *vm);
int renderer_waitforinput(C8State *vm);
int renderer_step(C8State *vm);

#endif