#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
//#include <SDL2/SDL_error.h>
//#include <SDL2/SDL_events.h>
//#include <SDL2/SDL_keysym.h>
//#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"

#define SCALE 8

#define TIMER_INTERVAL_MS 16

uint8 get_key(SDLKey key) {
  if (key == SDLK_1)
    return 0x1;
  if (key == SDLK_2)
    return 0x2;
  if (key == SDLK_3)
    return 0x3;
  if (key == SDLK_4)
    return 0xC;
  if (key == SDLK_a)
    return 0x4;
  if (key == SDLK_z)
    return 0x5;
  if (key == SDLK_e)
    return 0x6;
  if (key == SDLK_r)
    return 0xD;
  if (key == SDLK_q)
    return 0x7;
  if (key == SDLK_s)
    return 0x8;
  if (key == SDLK_d)
    return 0x9;
  if (key == SDLK_f)
    return 0xE;
  if (key == SDLK_w)
    return 0xA;
  if (key == SDLK_x)
    return 0x0;
  if (key == SDLK_c)
    return 0xB;
  if (key == SDLK_v)
    return 0xF;
  return 0;
}

void draw_screen(SDL_Surface *screen, cpu_t *cpu) {
  int x, y;
  SDL_Rect pixel;
  uint32 color;

  pixel.w = SCALE;
  pixel.h = SCALE;

  for (y = 0; y < SCREEN_HEIGHT; y++) {
    for (x = 0; x < SCREEN_WIDTH; x++) {
      uint8 pixel_value = cpu->video[x + (y * SCREEN_WIDTH)];
      pixel.x = x * SCALE;
      pixel.y = y * SCALE;

      color = pixel_value ? SDL_MapRGB(screen->format, 255, 255, 255)
                          : SDL_MapRGB(screen->format, 0, 0, 0);

      SDL_FillRect(screen, &pixel, color);
    }
  }

  SDL_Flip(screen);
}

int main(int argc, char **const argv) {
  SDL_Event event;
  SDL_Surface *screen = NULL;
  Mix_Chunk *beep;
  cpu_t *cpu = NULL;
  int running = 0;
  uint32_t last_timer_tick;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s rom_file_path\n", argv[0]);
    return 1;
  }

  if (SDL_Init(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) <
               0) == -1) {
    fprintf(stderr, "Cannot init SDL: %s", SDL_GetError());
    return 1;
  }

  screen = SDL_SetVideoMode(SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE, 32,
                            SDL_SWSURFACE);
  if (!screen) {
    fprintf(stderr, "Cannot set video mode: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  if (Mix_OpenAudio(22050, AUDIO_S16SYS, 2, 4096) < 0) {
    fprintf(stderr, "Unable to initialize SDL_mixer: %s\n", Mix_GetError());
    SDL_Quit();
    return 1;
  }

  beep = Mix_LoadWAV("assets/beep.wav");
  if (!beep) {
    fprintf(stderr, "Unable to load sound: %s\n", Mix_GetError());
    Mix_CloseAudio();
    SDL_Quit();
    return 1;
  }

  cpu = malloc(sizeof(cpu_t));
  if (!cpu) {
    fprintf(stderr, "Cannot allocate memory for the Chip-8 CPU");
    return 1;
  }

  reset(cpu);

  if (load_rom(cpu, argv[1]) == -1) {
    fprintf(stderr, "Cannot load ROM\n");
    free(cpu);
    return 1;
  }

  running = 1;
  last_timer_tick = SDL_GetTicks();

  while (running) {
    uint32_t now = 0;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_KEYDOWN:
        push_key(cpu, get_key(event.key.keysym.sym));
        break;
      case SDL_KEYUP:
        release_key(cpu, get_key(event.key.keysym.sym));
        break;
      case SDL_QUIT:
        running = 0;
        break;
      }
    }

    tick(cpu);

    now = SDL_GetTicks();
    if (now - last_timer_tick >= TIMER_INTERVAL_MS) {
      tick_timers(cpu);
      last_timer_tick = now;
    }

    if (cpu->draw_flag) {
      draw_screen(screen, cpu);
      cpu->draw_flag = FALSE;
    }

    if (cpu->sound_timer == 1) {
      Mix_PlayChannel(-1, beep, 0);
    }

    SDL_Delay(1);
  }

  SDL_Quit();

  free(cpu);
  return 0;
}
