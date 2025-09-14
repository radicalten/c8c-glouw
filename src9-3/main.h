#ifndef MAIN_H
#define MAIN_H

#include "ch8.h"

bool window_create();
void window_process_input(CH8_Core *cpu);
void window_render(CH8_Core *cpu);

#endif // MAIN_H
