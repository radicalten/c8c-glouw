#ifndef MAIN_H
#define MAIN_H

#include "ch8.h"

bool window_create();
void window_process_input(Ch8_display *cpu);
void window_render(cpu_t *cpu);

#endif // MAIN_H
