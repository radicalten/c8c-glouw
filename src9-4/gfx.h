#ifndef CHIP8_GFX
#define CHIP8_GFX

#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>

#include "chip.h"

#define PIXEL_SIZE 20
#define SCREEN_WIDTH (HGFX * PIXEL_SIZE)
#define SCREEN_HEIGHT (VGFX * PIXEL_SIZE)

void renderer_init();
void renderer_close();
void renderer_refresh(C8State *vm);
int renderer_waitforinput(C8State *vm);
int renderer_step(C8State *vm);

#endif