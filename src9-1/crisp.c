/*------------------------------------------------------------------
 *
 *  Copyright (C) 2021 Ashwin Godbole
 *
 *  Crisp is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation,
 *  either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  Crisp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE. See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crisp. If not, see <https://www.gnu.org/licenses/>.
 *
 *-----------------------------------------------------------------*/

#include "SDL2/SDL.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH 64
#define HEIGHT 32
#define PIXELSIZE 10

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;

enum key {
   k0,k1,k2,k3,
   k4,k5,k6,k7,
   k8,k9,ka,kb,
   kc,kd,ke,kf,
   none
};
typedef enum key Key;

struct chip {
   u8 v[16];
   i8 sp;
   u16 i;
   u16 pc;
   u8 mem[4096];
   u16 stack[16];
   u16 dtimer;
   u16 stimer;
   u8 display[HEIGHT * WIDTH];
   Key kp;
};
typedef struct chip Chip;
typedef void (*instrPointer)(Chip *, u16);

u16 
getReg1(u16 opcode) { return ((opcode & 0x0f00) >> 8); }

u16 
getReg2(u16 opcode) { return ((opcode & 0x00f0) >> 4); }

u16 
getAddr(u16 opcode) { return opcode & 0x0fff; }

u16 
get8BitVal(u16 opcode) { return opcode & 0x00ff; }

u16 
get4BitVal(u16 opcode) { return opcode & 0x000f; }

void 
reportError() { printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError()); }

void
drawPixel(u8 x, u8 y, SDL_Surface *surf) {
   SDL_Rect pix;
   pix.x = x*PIXELSIZE;
   pix.y = y*PIXELSIZE;
   pix.w = PIXELSIZE;
   pix.h = PIXELSIZE;
   SDL_FillRect(surf, &pix, SDL_MapRGB(surf->format, 0xFF, 0xFF, 0xFF));
}

void
refreshScreen(Chip *c, SDL_Surface *surf, SDL_Window *window) {
   int j;
   SDL_FillRect(surf, NULL, SDL_MapRGB(surf->format, 0x00, 0x00, 0x00));
   for (j = 0; j < WIDTH * HEIGHT; j++) {
      if (c->display[j]) {
         drawPixel(j%WIDTH, j/WIDTH, surf);
      }
   }
   SDL_UpdateWindowSurface(window);
}

/* instruction set implementation ========================= */
void
OP_0NNN(Chip *c, u16 opcode) {}

void
OP_00E0(Chip *c, u16 opcode) {
   int i;
   for (i = 0; i < WIDTH * HEIGHT; i++) {
      c->display[i] = (u8)0;
   }
}

void 
OP_00EE(Chip *c, u16 opcode) {
   if (c->sp < 0) {
      printf("ERROR: stack bounds exceeded. Stopping machine.\n");
      exit(1);
   }
   c->pc = c->stack[c->sp--];
}

void 
OP_1NNN(Chip *c, u16 opcode) { c->pc = getAddr(opcode); }

void 
OP_2NNN(Chip *c, u16 opcode) {
   if (c->sp > 16) {
      printf("ERROR: stack bounds exceeded. Stopping machine.\n");
      exit(1);
   }
   c->stack[++c->sp] = c->pc;
   c->pc = getAddr(opcode);
}

void 
OP_3XNN(Chip *c, u16 opcode) {
   if (c->v[getReg1(opcode)] == get8BitVal(opcode))
      c->pc += 2;
}

void 
OP_4XNN(Chip *c, u16 opcode) {
   if (c->v[getReg1(opcode)] != get8BitVal(opcode))
      c->pc += 2;
}

void 
OP_5XY0(Chip *c, u16 opcode) {
   if (c->v[getReg1(opcode)] == c->v[getReg2(opcode)])
      c->pc += 2;
}

void 
OP_6XNN(Chip *c, u16 opcode) {
   c->v[getReg1(opcode)] = (u8)get8BitVal(opcode);
}

void 
OP_7XNN(Chip *c, u16 opcode) {
   c->v[getReg1(opcode)] += (u8)get8BitVal(opcode);
}

void 
OP_8XY0(Chip *c, u16 opcode) {
   c->v[getReg1(opcode)] = c->v[getReg2(opcode)];
}

void 
OP_8XY1(Chip *c, u16 opcode) {
   c->v[getReg1(opcode)] |= c->v[getReg2(opcode)];
}

void 
OP_8XY2(Chip *c, u16 opcode) {
   c->v[getReg1(opcode)] &= c->v[getReg2(opcode)];
}

void 
OP_8XY3(Chip *c, u16 opcode) {
   c->v[getReg1(opcode)] ^= c->v[getReg2(opcode)];
}

void 
OP_8XY4(Chip *c, u16 opcode) {
   u16 sum;
   sum = c->v[getReg1(opcode)] + c->v[getReg2(opcode)];
   c->v[getReg1(opcode)] = (u8)sum;
   c->v[15] = (sum >= 0x100);
}

void 
OP_8XY5(Chip *c, u16 opcode) {
   if (c->v[getReg1(opcode)] > c->v[getReg2(opcode)]) {
      c->v[15] = 1;
   } else {
      c->v[15] = 0;
   }
   c->v[getReg1(opcode)] -= c->v[getReg2(opcode)];
}

void 
OP_8XY6(Chip *c, u16 opcode) {
   c->v[15] = c->v[getReg1(opcode)] & 0x1;
   c->v[getReg1(opcode)] >>= 1;
}

void 
OP_8XY7(Chip *c, u16 opcode) {
   if (c->v[getReg2(opcode)] > c->v[getReg1(opcode)]) {
      c->v[15] = 1;
   } else {
      c->v[15] = 0;
   }
   c->v[getReg1(opcode)] = c->v[getReg2(opcode)] - c->v[getReg1(opcode)];
}

void 
OP_8XYE(Chip *c, u16 opcode) {
   c->v[15] = c->v[getReg1(opcode)] >> 7;
   c->v[getReg1(opcode)] <<= 1;
}

void 
OP_9XY0(Chip *c, u16 opcode) {
   if (c->v[getReg1(opcode)] != c->v[getReg2(opcode)])
      c->pc += 2;
}

void 
OP_ANNN(Chip *c, u16 opcode) { c->i = getAddr(opcode); }

void 
OP_BNNN(Chip *c, u16 opcode) {
   c->pc = (c->v[0] + getAddr(opcode)) & 0x0fff;
}

void 
OP_CXNN(Chip *c, u16 opcode) {
   time_t t;
   srand((unsigned)time(&t));
   c->v[getReg1(opcode)] = (rand() % 256) & get8BitVal(opcode);
}

void 
OP_DXYN(Chip *c, u16 opcode) {
   u8 x, y, n, rowData, k, l;
   x = c->v[getReg1(opcode)] % WIDTH;
   y = c->v[getReg2(opcode)] % HEIGHT;
   n = (u8)get4BitVal(opcode);

   for (k = 0; k < n && y < HEIGHT; k++, y++) {
      rowData = c->mem[c->i + k];
      for (l = 0; l < 8 && (x + l) < WIDTH; l++) {
         c->display[x + l + y * WIDTH] ^= (u8)((rowData >> (7 - l)) & 0x1);
         c->v[15] = ((rowData >> (7 - l)) & 0x1) & (c->display[x + k + y * 64]);
      }
   }
}

void 
OP_EX9E(Chip *c, u16 opcode) {
   SDL_Event e;
   u8 x;
   x = c->v[getReg1(opcode)];
   if (x < 16 && c->kp == x) c->pc += 2;
   c->kp = none;
   while (SDL_PollEvent(&e));
}

void 
OP_EXA1(Chip *c, u16 opcode) {
   SDL_Event e;
   u8 x;
   x = c->v[getReg1(opcode)];
   if (x < 16 && c->kp != x) c->pc += 2;
   c->kp = none;
   while (SDL_PollEvent(&e));
}

void 
OP_FX07(Chip *c, u16 opcode) { c->v[getReg1(opcode)] = c->dtimer; }

void 
OP_FX0A(Chip *c, u16 opcode) {
   SDL_Event e;
   /* while (SDL_PollEvent(&e)); */
   SDL_WaitEvent(&e);
   if (e.type == SDL_KEYDOWN) {
      switch (e.key.keysym.sym) {
      case SDLK_1: c->v[getReg1(opcode)] = k1; return;
      case SDLK_2: c->v[getReg1(opcode)] = k2; return;
      case SDLK_3: c->v[getReg1(opcode)] = k3; return;
      case SDLK_q: c->v[getReg1(opcode)] = k4; return;
      case SDLK_w: c->v[getReg1(opcode)] = k5; return;
      case SDLK_e: c->v[getReg1(opcode)] = k6; return;
      case SDLK_a: c->v[getReg1(opcode)] = k7; return;
      case SDLK_s: c->v[getReg1(opcode)] = k8; return;
      case SDLK_d: c->v[getReg1(opcode)] = k9; return;
      case SDLK_z: c->v[getReg1(opcode)] = ka; return;
      case SDLK_x: c->v[getReg1(opcode)] = k0; return;
      case SDLK_c: c->v[getReg1(opcode)] = kb; return;
      case SDLK_4: c->v[getReg1(opcode)] = kc; return;
      case SDLK_r: c->v[getReg1(opcode)] = kd; return;
      case SDLK_f: c->v[getReg1(opcode)] = ke; return;
      case SDLK_v: c->v[getReg1(opcode)] = kf; return;
      }
   }
   /* c->pc-=2; */
   c->kp = none; 
   return;
}

void 
OP_FX15(Chip *c, u16 opcode) { c->dtimer = c->v[getReg1(opcode)]; }

void 
OP_FX18(Chip *c, u16 opcode) { c->stimer = c->v[getReg1(opcode)]; }

void 
OP_FX1E(Chip *c, u16 opcode) {
   c->i = (c->i + c->v[getReg1(opcode)]) & 0x0fff;
}

void 
OP_FX29(Chip *c, u16 opcode) {
   c->i = 0x50 + (5 * c->v[getReg1(opcode)]);
}

void 
OP_FX33(Chip *c, u16 opcode) {
   u16 temp, loc;
   temp = c->v[getReg1(opcode)];
   loc = c->i % 4096;
   c->mem[loc] = temp / 100;
   loc = (loc + 1) % 4096;
   c->mem[loc] = (temp / 10) % 10;
   loc = (loc + 1) % 4096;
   c->mem[c->i + 2] = temp % 10;
}

void 
OP_FX55(Chip *c, u16 opcode) {
   u16 tillReg, j;
   tillReg = getReg1(opcode);
   for (j = 0; j <= tillReg; j++) {
      c->mem[c->i + j] = c->v[j];
   }
}

void 
OP_FX65(Chip *c, u16 opcode) {
   u16 tillReg, j;
   tillReg = getReg1(opcode);
   for (j = 0; j <= tillReg; j++) {
      c->v[j] = c->mem[j + c->i];
   }
}

instrPointer Eight[8] = {
   &OP_8XY0, &OP_8XY1, &OP_8XY2, &OP_8XY3,
   &OP_8XY4, &OP_8XY5, &OP_8XY6, &OP_8XY7,
};

void 
OP_Zero(Chip *c, u16 opcode) {
   switch (opcode & 0x00ff) {
      case 0xe0:
         OP_00E0(c, opcode);
         break;
      case 0xee:
         OP_00EE(c, opcode);
         break;
      default:
         OP_0NNN(c, opcode);
   }
}

void 
OP_Eight(Chip *c, u16 opcode) {
   if ((opcode & 0xf) == 0xe)
      OP_8XYE(c, opcode);
   else
      (*Eight[opcode & 0xf])(c, opcode);
}

void 
OP_E(Chip *c, u16 opcode) {
   switch (opcode & 0x00ff) {
      case 0xa1:
         OP_EXA1(c, opcode);
         break;
      case 0x9e:
         OP_EX9E(c, opcode);
         break;
   }
}

void 
OP_F(Chip *c, u16 opcode) {
   switch (opcode & 0x00ff) {
      case 0x07:
         OP_FX07(c, opcode);
         break;
      case 0x0a:
         OP_FX0A(c, opcode);
         break;
      case 0x15:
         OP_FX15(c, opcode);
         break;
      case 0x18:
         OP_FX18(c, opcode);
         break;
      case 0x1e:
         OP_FX1E(c, opcode);
         break;
      case 0x29:
         OP_FX29(c, opcode);
         break;
      case 0x33:
         OP_FX33(c, opcode);
         break;
      case 0x55:
         OP_FX55(c, opcode);
         break;
      case 0x65:
         OP_FX65(c, opcode);
         break;
   }
}
/* end of instruction set implementation ================== */

instrPointer opTable[16] = {
   OP_Zero,  OP_1NNN, OP_2NNN, OP_3XNN, OP_4XNN, OP_5XY0, OP_6XNN, OP_7XNN,
   OP_Eight, OP_9XY0, OP_ANNN, OP_BNNN, OP_CXNN, OP_DXYN, OP_E,    OP_F,
};

int
sdlSetup() {
   if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      reportError();
      return 1;
   }
   return 0;
}

int 
loadProgram(Chip *c, char *filename) {
  FILE *f;
  int bRead;
  f = fopen(filename, "rb");
  if (!f)
    return 1;
  bRead = fread(c->mem + 512, 1, 4096 - 512, f);
  if (!bRead)
    return 1;
  return fclose(f);
}

void 
resetChip(Chip *c) {
  memset(c->mem, 0, 4096 * sizeof(u8));
  memset(c->v, 0, 16 * sizeof(u8));
  memset(c->stack, 0, 16 * sizeof(u16));
  memset(c->display, 0, WIDTH * HEIGHT * sizeof(u8));
  c->i = 0;
  c->sp = -1;
  c->pc = 0x200;
  c->dtimer = 0;
  c->stimer = 0;
  c->kp = none;
}

void
loadFont(Chip *c) {
   u8 fontStart, i;
   u8 font [80] = {
      0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
      0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
      0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
      0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
      0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
      0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
      0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
      0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
      0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
      0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
      0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
      0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
      0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
      0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
      0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
      0xF0, 0x80, 0xF0, 0x80, 0x80, /* F */
   };
   fontStart = 0x50;
   for (i = 0; i < 80; i++) c->mem[fontStart + i] = font[i];
}

int
main(int argc, char** argv) {
   int quit, err;
   u16 op;
   double curr, last;
   SDL_Event e;
   SDL_Window *window;
   SDL_Surface *surface;
   Chip c;
  
   if (argc != 2) {
      printf("usage : crisp <program>\n");
      return 1;
   }

   resetChip(&c);
   loadFont(&c);
   err = loadProgram(&c, argv[1]);

   if (err) {
      printf("ERROR: did not read or close file [ %s ]\n", argv[1]);
      return 1;
   }

   if (sdlSetup()) return 1;
   quit = 0;

   window = SDL_CreateWindow(
      "crisp", 
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      WIDTH*PIXELSIZE, HEIGHT*PIXELSIZE, SDL_WINDOW_SHOWN
   );
   if(!window) {
      reportError();
      return 1;
   }

   SDL_SetWindowResizable(window, SDL_FALSE);
   surface = SDL_GetWindowSurface( window );
   SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0x00, 0x00, 0x00));
   SDL_UpdateWindowSurface(window);
   curr = 0;
   last = 0;

   while (!quit) {
      while (SDL_PollEvent(&e)) {
         if (e.type == SDL_QUIT) quit = 1;
         else if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
            case SDLK_1: c.kp  = k1; break;
            case SDLK_2: c.kp  = k2; break;
            case SDLK_3: c.kp  = k3; break;
            case SDLK_q: c.kp  = k4; break;
            case SDLK_w: c.kp  = k5; break;
            case SDLK_e: c.kp  = k6; break;
            case SDLK_a: c.kp  = k7; break;
            case SDLK_s: c.kp  = k8; break;
            case SDLK_d: c.kp  = k9; break;
            case SDLK_z: c.kp  = ka; break;
            case SDLK_x: c.kp  = k0; break;
            case SDLK_c: c.kp  = kb; break;
            case SDLK_4: c.kp  = kc; break;
            case SDLK_r: c.kp  = kd; break;
            case SDLK_f: c.kp  = ke; break;
            case SDLK_v: c.kp  = kf; break;
            }                 
         }
      }

      curr = SDL_GetTicks();
      if (curr - last > 16.666) {
         last = curr - ((curr - last) - 16.666);
         c.dtimer = c.dtimer == 0 ? 0 : c.dtimer - 1;
         c.stimer = c.stimer == 0 ? 0 : c.stimer - 1;
      }

      if (c.pc > 4095) {
         printf("ran out of memory");
         return 1;
      }

      op = (c.mem[c.pc] << 8) + c.mem[c.pc + 1];
      c.pc += 2;
      (*opTable[(op & 0xf000) >> 12])(&c, op);

      refreshScreen(&c, surface, window);
      SDL_Delay(1);
   }

   SDL_DestroyWindow(window);
   SDL_Quit();
   return 0;
}

