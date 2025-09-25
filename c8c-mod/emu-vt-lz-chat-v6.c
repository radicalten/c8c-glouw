#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

// Display and memory configuration
#define VROWS (32)
#define VCOLS (64)
#define BYTES (4096)
#define START (0x0200)
#define VSIZE (16)
#define SSIZE (12)
#define BFONT (80)

// Virtual-time configuration
#define CPU_HZ     700.0
#define TIMER_HZ     60.0
#define REFRESH_HZ   60.0

static uint64_t vmem[VROWS];

static uint16_t pc = START;
static uint16_t I;
static uint16_t s[SSIZE];
static uint16_t op;

static uint8_t dt;
static uint8_t st;
static uint8_t sp;
static uint8_t v[VSIZE];
static uint8_t mem[BYTES];
static uint8_t charges[VROWS][VCOLS];

static SDL_Window* window;
static SDL_Renderer* renderer;

// Lazy draw flag + FX0A wait state
static int draw_flag = 1;
static int waiting_for_key = 0;
static uint8_t wait_reg = 0xFF;

// Virtual-time accumulators
static uint64_t perf_freq = 0;
static uint64_t last_counter = 0;
static double cycles_accum = 0.0;
static double timer_accum  = 0.0;
static double frame_accum  = 0.0;

// Map CHIP-8 keypad code (0x0..0xF) to SDL scancode
static int scancode_for_chip8(uint8_t code) {
    switch (code) {
        case 0x1: return SDL_SCANCODE_1;
        case 0x2: return SDL_SCANCODE_2;
        case 0x3: return SDL_SCANCODE_3;
        case 0xC: return SDL_SCANCODE_4;
        case 0x4: return SDL_SCANCODE_Q;
        case 0x5: return SDL_SCANCODE_W;
        case 0x6: return SDL_SCANCODE_E;
        case 0xD: return SDL_SCANCODE_R;
        case 0x7: return SDL_SCANCODE_A;
        case 0x8: return SDL_SCANCODE_S;
        case 0x9: return SDL_SCANCODE_D;
        case 0xE: return SDL_SCANCODE_F;
        case 0xA: return SDL_SCANCODE_Z;
        case 0x0: return SDL_SCANCODE_X;
        case 0xB: return SDL_SCANCODE_C;
        case 0xF: return SDL_SCANCODE_V;
        default:  return -1;
    }
}

// Check if a specific CHIP-8 key is currently down
static int chip8_key_down(uint8_t code) {
    SDL_PumpEvents();
    const Uint8* st = SDL_GetKeyboardState(NULL);
    int sc = scancode_for_chip8(code);
    return (sc >= 0) ? (st[sc] != 0) : 0;
}

// Return any currently pressed CHIP-8 key (0x0..0xF) or -1 if none
static int any_chip8_key_pressed(void) {
    SDL_PumpEvents();
    const Uint8* st = SDL_GetKeyboardState(NULL);
    struct { uint8_t code; SDL_Scancode sc; } map[] = {
        {0x1, SDL_SCANCODE_1}, {0x2, SDL_SCANCODE_2}, {0x3, SDL_SCANCODE_3}, {0xC, SDL_SCANCODE_4},
        {0x4, SDL_SCANCODE_Q}, {0x5, SDL_SCANCODE_W}, {0x6, SDL_SCANCODE_E}, {0xD, SDL_SCANCODE_R},
        {0x7, SDL_SCANCODE_A}, {0x8, SDL_SCANCODE_S}, {0x9, SDL_SCANCODE_D}, {0xE, SDL_SCANCODE_F},
        {0xA, SDL_SCANCODE_Z}, {0x0, SDL_SCANCODE_X}, {0xB, SDL_SCANCODE_C}, {0xF, SDL_SCANCODE_V},
    };
    for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); ++i) {
        if (st[map[i].sc]) return map[i].code;
    }
    return -1;
}

// Backward-compatible helper (used in main’s wait loop)
static int input(const int waiting)
{
    int k;
    do {
        k = any_chip8_key_pressed();
        if (k != -1) return k;
        if (!waiting) break;
        SDL_Delay(1);
    } while (waiting);
    return -1;
}

static void _0000() { /* no-op */ }
static void _00E0() { for(int j = 0; j < VROWS; j++) while(vmem[j] >>= 1); draw_flag = 1; }
static void _00EE() { pc = s[--sp]; }
static void _1NNN() { uint16_t nnn = op & 0x0FFF; pc = nnn; }
static void _2NNN() { uint16_t nnn = op & 0x0FFF; s[sp++] = pc; pc = nnn; }
static void _3XNN() { uint16_t x = (op & 0x0F00) >> 8, nn = op & 0x00FF; if(v[x] == nn) pc += 0x0002; }
static void _4XNN() { uint16_t x = (op & 0x0F00) >> 8, nn = op & 0x00FF; if(v[x] != nn) pc += 0x0002; }
static void _5XY0() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; if(v[x] == v[y]) pc += 0x0002; }
static void _6XNN() { uint16_t nn = op & 0x00FF, x = (op & 0x0F00) >> 8; v[x]  = nn; }
static void _7XNN() { uint16_t nn = op & 0x00FF, x = (op & 0x0F00) >> 8; v[x] += nn; }
static void _8XY0() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; v[x]  = v[y]; }
static void _8XY1() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; v[x] |= v[y]; }
static void _8XY2() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; v[x] &= v[y]; }
static void _8XY3() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; v[x] ^= v[y]; }
static void _8XY4() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; uint8_t flag = v[x] + v[y] > 0xFF ? 0x01 : 0x00; v[x] = v[x] + v[y]; v[0xF] = flag; }
static void _8XY5() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; uint8_t flag = v[x] - v[y] < 0x00 ? 0x00 : 0x01; v[x] = v[x] - v[y]; v[0xF] = flag; }
static void _8XY7() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; uint8_t flag = v[y] - v[x] < 0x00 ? 0x00 : 0x01; v[x] = v[y] - v[x]; v[0xF] = flag; }
static void _8XY6() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; uint8_t flag = (v[y] >> 0) & 0x01; v[x] = v[y] >> 1; v[0xF] = flag; }
static void _8XYE() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; uint8_t flag = (v[y] >> 7) & 0x01; v[x] = v[y] << 1; v[0xF] = flag; }
static void _9XY0() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; if(v[x] != v[y]) pc += 0x0002; }
static void _ANNN() { uint16_t nnn = op & 0x0FFF; I = nnn; }
static void _BNNN() { uint16_t nnn = op & 0x0FFF; pc = nnn + v[0x0]; }
static void _CXNN() { uint16_t x = (op & 0x0F00) >> 8, nn = op & 0x00FF; v[x] = nn & (rand() % 0x100); }
static void _DXYN() {
    uint16_t x = (op & 0x0F00) >> 8;
    uint16_t y = (op & 0x00F0) >> 4;
    uint16_t n = (op & 0x000F) >> 0;
    uint8_t flag = 0;
    for(int j = 0; j < n; j++)
    {
        if (v[y] + j >= VROWS) continue; // basic clip to avoid OOB
        uint64_t line = (uint64_t) mem[I + j] << (VCOLS - 8);
        line >>= (v[x] % VCOLS);
        if((vmem[v[y] + j] ^ line) != (vmem[v[y] + j] | line))
            flag = 1;
        vmem[v[y] + j] ^= line;
    }
    v[0xF] = flag;
    draw_flag = 1; // mark screen dirty
}
static void _EXA1() { uint16_t x = (op & 0x0F00) >> 8; if(!chip8_key_down(v[x])) pc += 0x0002; } // SKNP Vx
static void _EX9E() { uint16_t x = (op & 0x0F00) >> 8; if( chip8_key_down(v[x])) pc += 0x0002; } // SKP Vx
static void _FX07() { uint16_t x = (op & 0x0F00) >> 8; v[x] = dt; }
static void _FX0A() { uint16_t x = (op & 0x0F00) >> 8; waiting_for_key = 1; wait_reg = (uint8_t)x; }
static void _FX15() { uint16_t x = (op & 0x0F00) >> 8; dt = v[x]; }
static void _FX18() { uint16_t x = (op & 0x0F00) >> 8; st = v[x]; }
static void _FX1E() { uint16_t x = (op & 0x0F00) >> 8; I += v[x]; }
static void _FX29() { uint16_t x = (op & 0x0F00) >> 8; I = 5 * v[x]; }
static void _FX33() { uint16_t x = (op & 0x0F00) >> 8;
    const int lookup[] = { 100, 10, 1 };
    for(unsigned i = 0; i < sizeof(lookup) / sizeof(*lookup); i++)
        mem[I + i] = v[x] / lookup[i] % 10;
}
static void _FX55() { uint16_t x = (op & 0x0F00) >> 8; int i; for(i = 0; i <= x; i++) mem[I + i] = v[i]; I += i; }
static void _FX65() { uint16_t x = (op & 0x0F00) >> 8; int i; for(i = 0; i <= x; i++) v[i] = mem[I + i]; I += i; }

static void _0___();
static void _8___();
static void _E___();
static void _F___();
static void (*opsa[])() = { _00E0, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _00EE, _0000 };
static void (*opsb[])() = { _8XY0, _8XY1, _8XY2, _8XY3, _8XY4, _8XY5, _8XY6, _8XY7, _0000, _0000, _0000, _0000, _0000, _0000, _8XYE, _0000 };
static void (*opsc[])() = { _0000, _EXA1, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _EX9E, _0000 };
static void (*opsd[])() = { _0000, _0000, _0000, _0000, _0000, _FX07, _0000, _0000, _FX0A, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*************************/ _0000, _0000, _0000, _0000, _0000, _FX15, _0000, _0000, _FX18, _0000, _0000, _0000, _0000, _0000, _FX1E, _0000,
/*                       */ _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _FX29, _0000, _0000, _0000, _0000, _0000, _0000,
/*                       */ _0000, _0000, _0000, _FX33, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*         CHIP-8        */ _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*                       */ _0000, _0000, _0000, _0000, _0000, _FX55, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*                       */ _0000, _0000, _0000, _0000, _0000, _FX65, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*************************/ _0000, _0000, _0000, _0000, _0000, _FX65, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000 };
static void (*exec[])() = { _0___, _1NNN, _2NNN, _3XNN, _4XNN, _5XY0, _6XNN, _7XNN, _8___, _9XY0, _ANNN, _BNNN, _CXNN, _DXYN, _E___, _F___ };
static void _0___() { (*opsa[op & 0x000F])(); }
static void _8___() { (*opsb[op & 0x000F])(); }
static void _E___() { (*opsc[op & 0x000F])(); }
static void _F___() { (*opsd[op & 0x00FF])(); }

static void load(const char* game)
{
    const uint8_t ch[BFONT] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,
        0x20, 0x60, 0x20, 0x20, 0x70,
        0xF0, 0x10, 0xF0, 0x80, 0xF0,
        0xF0, 0x10, 0xF0, 0x10, 0xF0,
        0x90, 0x90, 0xF0, 0x10, 0x10,
        0xF0, 0x80, 0xF0, 0x10, 0xF0,
        0xF0, 0x80, 0xF0, 0x90, 0xF0,
        0xF0, 0x10, 0x20, 0x40, 0x40,
        0xF0, 0x90, 0xF0, 0x90, 0xF0,
        0xF0, 0x90, 0xF0, 0x10, 0xF0,
        0xF0, 0x90, 0xF0, 0x90, 0x90,
        0xE0, 0x90, 0xE0, 0x90, 0xE0,
        0xF0, 0x80, 0x80, 0x80, 0xF0,
        0xE0, 0x90, 0x90, 0x90, 0xE0,
        0xF0, 0x80, 0xF0, 0x80, 0xF0,
        0xF0, 0x80, 0xF0, 0x80, 0x80,
    };
    FILE* const fp = fopen(game, "rb");
    if(fp == NULL)
    {
        fprintf(stderr, "error: binary '%s' not found\n", game);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    if (fsize < 0) { fclose(fp); fprintf(stderr, "error: failed to get file size\n"); exit(1); }
    rewind(fp);
    if (fsize > (long)(BYTES - START)) fsize = BYTES - START;

    uint8_t buf[BYTES];
    size_t nread = fread(buf, 1, (size_t)fsize, fp);
    fclose(fp);

    if (nread != (size_t)fsize) {
        fprintf(stderr, "error: failed to read binary\n");
        exit(1);
    }

    for(int i = 0; i < BFONT; i++)
        mem[i] = ch[i];
    for(int i = 0; i < (int)fsize; i++)
        mem[i + START] = buf[i];
}

static void cycle()
{
    // Halt instruction execution if waiting for FX0A; timers/frames continue elsewhere.
    if (waiting_for_key) return;

    op = (mem[pc] << 8) + (mem[pc + 1] & 0x00FF);
    pc += 0x0002;
    (*exec[op >> 12])();
}

static void output()
{
    // Clear to black
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(renderer);

    // Draw white pixels for "on" cells
    const int w = 8;
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    for(int j = 0; j < VROWS; j++)
    for(int i = 0; i < VCOLS; i++)
    {
        if (charges[j][i]) {
            const SDL_Rect rect = { i * w, j * w, w, w };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    SDL_RenderPresent(renderer);
}

static int charging(const int j, const int i)
{
    return (vmem[j] >> (VCOLS - 1 - i)) & 0x1;
}

static void charge()
{
    // Rebuild the entire charge buffer from vmem (persistent until next draw/clear)
    for(int j = 0; j < VROWS; j++)
    for(int i = 0; i < VCOLS; i++)
        charges[j][i] = charging(j, i) ? 0xFF : 0x00;
}

void dump()
{
    for(int i = 0; i < VSIZE; i++)
        printf("v[%02d]: %d = 0x%02X\n", i, v[i], v[i]);
}

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        fprintf(stderr, "error: too few or too many arguments\n");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }
    if (SDL_CreateWindowAndRenderer(512, 256, 0, &window, &renderer) != 0) {
        fprintf(stderr, "SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
    SDL_SetWindowTitle(window, "Emu-1.0");
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    load(argv[1]);
    srand((unsigned)time(0));

    perf_freq    = SDL_GetPerformanceFrequency();
    last_counter = SDL_GetPerformanceCounter();
    cycles_accum = timer_accum = frame_accum = 0.0;

    int running = 1;
    SDL_Event e;

    while (running)
    {
        // Handle events (close window or press Esc/End to quit)
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE ||
                    e.key.keysym.scancode == SDL_SCANCODE_END) {
                    running = 0;
                }
            }
        }

        // If an FX0A is pending, latch the next key without blocking.
        if (waiting_for_key) {
            int k = input(0);
            if (k != -1) {
                v[wait_reg] = (uint8_t)k;
                waiting_for_key = 0;
            }
        }

        // Virtual time step
        uint64_t now = SDL_GetPerformanceCounter();
        double dt_s = (double)(now - last_counter) / (double)perf_freq;
        last_counter = now;

        cycles_accum += dt_s * CPU_HZ;
        timer_accum  += dt_s * TIMER_HZ;
        frame_accum  += dt_s * REFRESH_HZ;

        // Run CPU cycles at CPU_HZ, but halt if waiting for key
        if (!waiting_for_key) {
            int cycles_to_run = (int)cycles_accum;
            if (cycles_to_run > 0) {
                cycles_accum -= cycles_to_run;
                for (int i = 0; i < cycles_to_run; ++i) {
                    cycle();
                }
            }
        } else {
            // Do not accumulate backlog while waiting on FX0A
            cycles_accum = 0.0;
        }

        // Tick timers at 60 Hz
        while (timer_accum >= 1.0) {
            if (dt > 0) dt--;
            if (st > 0) st--;
            // if (st) { /* Beep */ }
            timer_accum -= 1.0;
        }

        // Refresh at REFRESH_HZ
        while (frame_accum >= 1.0) {
            if (draw_flag) {
                // Rebuild the entire charge buffer from vmem
                charge();
                draw_flag = 0;
            }
            output();
            // Do NOT discharge: leave pixels until next draw/clear
            frame_accum -= 1.0;
        }

        SDL_Delay(1);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

/* Prompt: gpt-5-high Can you fix this file so that it reads keyboard inputs properly? currently when keys are pressed nothing happens:
Prompt2: can you put this in one C file?
You're reading “some random pressed key” and comparing it to Vx for SKP/SKNP, instead of checking whether “the specific key referenced by Vx” is down. That makes most CHIP‑8 games ignore your input. Also, you cached the keyboard state pointer once; it’s safer to fetch it each time you query keys.

Below is a minimal patch that:

    Uses the correct semantics for EX9E (SKP Vx) and EXA1 (SKNP Vx).
    Queries SDL’s keyboard state when needed (no stale pointer).
    Keeps your FX0A wait logic, but uses the same consistent key mapping.

Apply this diff (or mirror the same changes manually).

Diff (unified)

Why this fixes it

    SKP/SKNP now correctly check “is key Vx down?” instead of “does any pressed key equal Vx right now?”, which is what CHIP‑8 programs expect when polling input.
    Keyboard state is fetched from SDL each time we query, after SDL_PumpEvents, so we don’t rely on a cached pointer or stale state.
    FX0A still waits for any key and stores the correct CHIP‑8 keycode (0x0..0xF).

Tip

    Make sure the emulator window has focus while testing.
    The mapping is the classic CHIP‑8 layout:
    1 2 3 4
    Q W E R
    A S D F
    Z X C V
    where X = 0, 1..F → 1,2,3,4,Q,W,E,R,A,S,D,F,Z,X,C,V respectively.

If you want, I can also fold SKP/SKNP to use an internal 16-element keypad state updated from SDL_KEYDOWN/KEYUP events, but the above is the smallest change to get your inputs working as expected.

Here’s the fixed, single-file C program with correct CHIP-8 input handling (SKP/SKNP check the specific key in Vx; FX0A waits for any key; SDL keyboard state is fetched fresh each time):
*/
