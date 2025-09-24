#include <SDL2/SDL.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VROWS (32)
#define VCOLS (64)
#define BYTES (4096)
#define START (0x0200)
#define VSIZE (16)
#define SSIZE (12)
#define BFONT (80)

// Virtual timing parameters (adjust VCPU_HZ to taste)
#define VCPU_HZ  (600u)  // virtual CPU cycles/sec
#define TIMER_HZ (60u)   // CHIP-8 timers tick at 60 Hz
#define VIDEO_HZ (60u)   // virtual "frame rate" for output

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

static const uint8_t* key;

static SDL_Window* window;
static SDL_Renderer* renderer;

// Virtual time accumulators
static uint32_t acc_timer = 0;
static uint32_t acc_video = 0;

/* -------------------- Lazy VF evaluation -------------------- */
typedef enum {
    FLAG_NONE = 0,
    FLAG_ADD,
    FLAG_SUBXY,
    FLAG_SUBYX,
    FLAG_SHR,
    FLAG_SHL
} flag_kind_t;

static struct {
    flag_kind_t kind;
    uint8_t a;  // operand or source value
    uint8_t b;  // second operand (when needed)
} lazy_vf = { FLAG_NONE, 0, 0 };

static inline void lazy_vf_clear(void) { lazy_vf.kind = FLAG_NONE; }

static inline void lazy_vf_set_add(uint8_t a, uint8_t b)   { lazy_vf.kind = FLAG_ADD;   lazy_vf.a = a; lazy_vf.b = b; }
static inline void lazy_vf_set_subxy(uint8_t a, uint8_t b) { lazy_vf.kind = FLAG_SUBXY; lazy_vf.a = a; lazy_vf.b = b; }
static inline void lazy_vf_set_subyx(uint8_t a, uint8_t b) { lazy_vf.kind = FLAG_SUBYX; lazy_vf.a = a; lazy_vf.b = b; }
static inline void lazy_vf_set_shr(uint8_t a)              { lazy_vf.kind = FLAG_SHR;   lazy_vf.a = a; }
static inline void lazy_vf_set_shl(uint8_t a)              { lazy_vf.kind = FLAG_SHL;   lazy_vf.a = a; }

static inline void lazy_vf_realize(void)
{
    switch (lazy_vf.kind)
    {
        case FLAG_ADD:   v[0xF] = ((uint16_t)lazy_vf.a + (uint16_t)lazy_vf.b) > 0xFFu; break;
        case FLAG_SUBXY: v[0xF] = (lazy_vf.a >= lazy_vf.b) ? 1u : 0u; break;
        case FLAG_SUBYX: v[0xF] = (lazy_vf.b >= lazy_vf.a) ? 1u : 0u; break;
        case FLAG_SHR:   v[0xF] = (lazy_vf.a & 0x01u); break;
        case FLAG_SHL:   v[0xF] = (lazy_vf.a >> 7) & 0x01u; break;
        case FLAG_NONE:  break;
    }
    lazy_vf.kind = FLAG_NONE;
}

static inline uint8_t READV(uint16_t r) { if (r == 0xF) lazy_vf_realize(); return v[r]; }
static inline void    WRITEV(uint16_t r, uint8_t val) { if (r == 0xF) lazy_vf_clear(); v[r] = val; }
static inline void    WRITEVF(uint8_t val) { lazy_vf_clear(); v[0xF] = val; }
/* ----------------------------------------------------------- */

static int input(const int waiting)
{
    do
    {
        SDL_PumpEvents();
        if(key[SDL_SCANCODE_1]) return 0x01;
        if(key[SDL_SCANCODE_2]) return 0x02;
        if(key[SDL_SCANCODE_3]) return 0x03;
        if(key[SDL_SCANCODE_4]) return 0x0C;
        if(key[SDL_SCANCODE_Q]) return 0x04;
        if(key[SDL_SCANCODE_W]) return 0x05;
        if(key[SDL_SCANCODE_E]) return 0x06;
        if(key[SDL_SCANCODE_R]) return 0x0D;
        if(key[SDL_SCANCODE_A]) return 0x07;
        if(key[SDL_SCANCODE_S]) return 0x08;
        if(key[SDL_SCANCODE_D]) return 0x09;
        if(key[SDL_SCANCODE_F]) return 0x0E;
        if(key[SDL_SCANCODE_Z]) return 0x0A;
        if(key[SDL_SCANCODE_X]) return 0x00;
        if(key[SDL_SCANCODE_C]) return 0x0B;
        if(key[SDL_SCANCODE_V]) return 0x0F;
    }
    while(waiting);
    return -1;
}

static void _0000() { /* no-op */ }
static void _00E0() { for(int j = 0; j < VROWS; j++) while(vmem[j] >>= 1); }
static void _00EE() { pc = s[--sp]; }
static void _1NNN() { uint16_t nnn = op & 0x0FFF; pc = nnn; }
static void _2NNN() { uint16_t nnn = op & 0x0FFF; s[sp++] = pc; pc = nnn; }
static void _3XNN() { uint16_t x = (op & 0x0F00) >> 8, nn = op & 0x00FF; if(READV(x) == nn) pc += 0x0002; }
static void _4XNN() { uint16_t x = (op & 0x0F00) >> 8, nn = op & 0x00FF; if(READV(x) != nn) pc += 0x0002; }
static void _5XY0() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; if(READV(x) == READV(y)) pc += 0x0002; }
static void _6XNN() { uint16_t nn = op & 0x00FF, x = (op & 0x0F00) >> 8; WRITEV(x, (uint8_t)nn); }
static void _7XNN() { uint16_t nn = op & 0x00FF, x = (op & 0x0F00) >> 8; uint8_t vx = READV(x); WRITEV(x, (uint8_t)(vx + nn)); }
static void _8XY0() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; WRITEV(x, READV(y)); }
static void _8XY1() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; uint8_t vx = READV(x), vy = READV(y); WRITEV(x, vx | vy); }
static void _8XY2() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; uint8_t vx = READV(x), vy = READV(y); WRITEV(x, vx & vy); }
static void _8XY3() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; uint8_t vx = READV(x), vy = READV(y); WRITEV(x, vx ^ vy); }
static void _8XY4() {
    uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4;
    uint8_t vx = READV(x), vy = READV(y);
    uint16_t sum = (uint16_t)vx + (uint16_t)vy;
    WRITEV(x, (uint8_t)sum);
    lazy_vf_set_add(vx, vy);
}
static void _8XY5() {
    uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4;
    uint8_t vx = READV(x), vy = READV(y);
    WRITEV(x, (uint8_t)(vx - vy));
    lazy_vf_set_subxy(vx, vy);
}
static void _8XY7() {
    uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4;
    uint8_t vx = READV(x), vy = READV(y);
    WRITEV(x, (uint8_t)(vy - vx));
    lazy_vf_set_subyx(vx, vy);
}
static void _8XY6() {
    uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4;
    uint8_t vy = READV(y);
    WRITEV(x, (uint8_t)(vy >> 1));
    lazy_vf_set_shr(vy);
}
static void _8XYE() {
    uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4;
    uint8_t vy = READV(y);
    WRITEV(x, (uint8_t)(vy << 1));
    lazy_vf_set_shl(vy);
}
static void _9XY0() { uint16_t x = (op & 0x0F00) >> 8, y = (op & 0x00F0) >> 4; if(READV(x) != READV(y)) pc += 0x0002; }
static void _ANNN() { uint16_t nnn = op & 0x0FFF; I = nnn; }
static void _BNNN() { uint16_t nnn = op & 0x0FFF; pc = nnn + v[0x0]; }
static void _CXNN() { uint16_t x = (op & 0x0F00) >> 8, nn = op & 0x00FF; WRITEV(x, (uint8_t)(nn & (rand() % 0x100))); }
static void _DXYN() {
    uint16_t x = (op & 0x0F00) >> 8;
    uint16_t y = (op & 0x00F0) >> 4;
    uint16_t n = (op & 0x000F) >> 0;
    uint8_t flag = 0;
    uint8_t vx = READV(x);
    uint8_t vy = READV(y);
    for(int j = 0; j < n; j++)
    {
        uint64_t line = (uint64_t) mem[I + j] << (VCOLS - 8);
        line >>= vx;
        if((vmem[vy + j] ^ line) != (vmem[vy + j] | line))
            flag = 1;
        vmem[vy + j] ^= line;
    }
    WRITEVF(flag); // draw collision is realized immediately
}
static void _EXA1() { uint16_t x = (op & 0x0F00) >> 8; if(READV(x) != input(0)) pc += 0x0002; }
static void _EX9E() { uint16_t x = (op & 0x0F00) >> 8; if(READV(x) == input(0)) pc += 0x0002; }
static void _FX07() { uint16_t x = (op & 0x0F00) >> 8; WRITEV(x, dt); }
static void _FX0A() { uint16_t x = (op & 0x0F00) >> 8; WRITEV(x, (uint8_t)input(1)); }
static void _FX15() { uint16_t x = (op & 0x0F00) >> 8; dt = READV(x); }
static void _FX18() { uint16_t x = (op & 0x0F00) >> 8; st = READV(x); }
static void _FX1E() { uint16_t x = (op & 0x0F00) >> 8; I += READV(x); }
static void _FX29() { uint16_t x = (op & 0x0F00) >> 8; I = 5 * READV(x); }
static void _FX33() {
    uint16_t x = (op & 0x0F00) >> 8;
    uint8_t vx = READV(x);
    const int lookup[] = { 100, 10, 1 };
    for(unsigned i = 0; i < sizeof(lookup) / sizeof(*lookup); i++)
        mem[I + i] = (uint8_t)((vx / lookup[i]) % 10);
}
static void _FX55() {
    uint16_t x = (op & 0x0F00) >> 8; int i;
    for(i = 0; i <= x; i++) mem[I + i] = READV((uint16_t)i);
    I += i;
}
static void _FX65() {
    uint16_t x = (op & 0x0F00) >> 8; int i;
    for(i = 0; i <= x; i++) WRITEV((uint16_t)i, mem[I + i]);
    I += i;
}

static void _0___();
static void _8___();
static void _E___();
static void _F___();
static void (*opsa[])() = { _00E0, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _00EE, _0000 };
static void (*opsb[])() = { _8XY0, _8XY1, _8XY2, _8XY3, _8XY4, _8XY5, _8XY6, _8XY7, _0000, _0000, _0000, _0000, _0000, _0000, _8XYE, _0000 };
static void (*opsc[])() = { _0000, _EXA1, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _EX9E, _0000 };
static void (*opsd[])() = { _0000, _0000, _0000, _0000, _0000, _FX07, _0000, _0000, _FX0A, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*************************/ _0000, _0000, _0000, _0000, _FX15, _0000, _0000, _FX18, _0000, _0000, _0000, _0000, _0000, _FX1E, _0000, _0000,
/*                       */ _0000, _0000, _0000, _0000, _0000, _0000, _FX29, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*                       */ _0000, _0000, _FX33, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*         CHIP-8        */ _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*                       */ _0000, _0000, _0000, _0000, _FX55, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*                       */ _0000, _0000, _0000, _0000, _FX65, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000,
/*************************/ _0000, _0000, _0000, _0000, _FX65, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000, _0000 };
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
    const long size = ftell(fp);
    rewind(fp);
    uint8_t buf[BYTES];
    fread(buf, 1, size, fp);
    fclose(fp);
    for(int i = 0; i < BFONT; i++)
        mem[i] = ch[i];
    for(int i = 0; i < size; i++)
        mem[i + START] = buf[i];
}

static void cycle()
{
    // Purely virtual: fetch-decode-execute only (no real-time tick here)
    op = (mem[pc] << 8) + (mem[pc + 1] & 0x00FF);
    pc += 0x0002;
    (*exec[op >> 12])();
}

static void tick_timers(void)
{
    if(dt > 0) dt--;
    if(st > 0) st--;
    if(st)
    {
        /* Beep */
    }
}

static void output()
{
    for(int j = 0; j < VROWS; j++)
    for(int i = 0; i < VCOLS; i++)
    {
        SDL_SetRenderDrawColor(renderer, charges[j][i], 0x00, 0x00, 0x00);
        const int w = 8;
        const SDL_Rect rect = { i * w + 1, j * w + 1, w - 2, w - 2 };
        SDL_RenderFillRect(renderer, &rect);
    }
    SDL_RenderPresent(renderer);
}

static int charging(const int j, const int i)
{
    return (vmem[j] >> (VCOLS - 1 - i)) & 0x1;
}

static void charge()
{
    for(int j = 0; j < VROWS; j++)
    for(int i = 0; i < VCOLS; i++)
        if(charging(j, i))
            charges[j][i] = 0xFF;
}

static void discharge()
{
    // Single-frame persistence: clear the entire charge buffer once per frame
    memset(charges, 0x00, sizeof(charges));
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
        fprintf(stderr, "error: too few or too many argmuents\n");
        exit(1);
    }
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(512, 256, 0, &window, &renderer);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_SetWindowTitle(window, "Emu-1.0");
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    key = SDL_GetKeyboardState(NULL);
    load(argv[1]);
    srand(time(0));

    for(uint64_t cycles = 0; !key[SDL_SCANCODE_END] && !key[SDL_SCANCODE_ESCAPE]; cycles++)
    {
        SDL_PumpEvents(); // Keep keyboard state fresh

        // One virtual CPU cycle
        cycle();

        // Update display buffers based on current vmem
        charge();

        // Virtual 60 Hz timer ticks
        acc_timer += TIMER_HZ;
        while (acc_timer >= VCPU_HZ) {
            acc_timer -= VCPU_HZ;
            tick_timers();
        }

        // Virtual 60 Hz video updates
        acc_video += VIDEO_HZ;
        while (acc_video >= VCPU_HZ) {
            acc_video -= VCPU_HZ;
            output();
            discharge(); // instant off for non-charging pixels
        }
    }
    SDL_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}


/* gpt-5-high
    Nice! Below is your code updated to use lazy flag evaluation for VF. The emulator now defers computing VF for ADD/SUB/SHR/SHL until VF is actually read. Reading VF (as a source register) triggers realization; writing to VF cancels any pending lazy flag. DXYN (sprite draw) still computes VF immediately for correctness without redoing the blit.

What changed (brief):
- Added a tiny lazy-VF engine: defer setting VF after 8XY4/5/7/6/E; compute only when V[F] is read.
- Added READV/WRITEV helpers to auto-realize/cancel lazy flags when reading/writing V[F].
- Updated all op handlers that read/write registers to use READV/WRITEV where relevant.



Notes:
- VF is now computed lazily for 8XY4/5/7/6/E. If a program never reads VF before it’s overwritten, we never compute it.
- Any read of V[F] via X=F or Y=F in ops like 3XNN/4XNN/5XY0/9XY0/8XYN/FX55 triggers realization.
- Any write to V[F] cancels the pending lazy flag.
- DXYN keeps immediate VF collision semantics for correctness.

Want me to convert this into a unified diff instead?
    */


