#ifndef CHIP8_CHIP
#define CHIP8_CHIP

#include <inttypes.h>

/*
    This was based off of the specification given by http://www.cs.columbia.edu/~sedwards/classes/2016/4840-spring/designs/Chip8.pdf
*/

#define VREGISTERS  16
#define STACKSIZE   64
#define FONTADDR    0x000
#define STARTPC     0x200
#define RAMSIZE     0x1000
#define VGFX        32
#define HGFX        64
#define GFXSIZE     (VGFX*HGFX)

#define CLOCK_HZ    48
#define CLOCK_RATE  ((int)((1.0 / CLOCK_HZ) * 1000))

/* define some macros to grab the arguments in each 16bit instruction  */
#define GET_X000(i) ((i & 0xF000) >> 12)
#define GET_0X00(i) ((i & 0x0F00) >> 8)
#define GET_00X0(i) ((i & 0x00F0) >> 4)
#define GET_000X(i) (i & 0x000F)
#define GET_0XX0(i) ((i & 0x0FF0) >> 4)
#define GET_00XX(i) (i & 0x00FF)
#define GET_0XXX(i) (i & 0x0FFF)

typedef enum {
    C8KEY_1,
    C8KEY_2,
    C8KEY_3,
    C8KEY_C,

    C8KEY_4,
    C8KEY_5,
    C8KEY_6,
    C8KEY_D,

    C8KEY_7,
    C8KEY_8,
    C8KEY_9,
    C8KEY_E,

    C8KEY_A,
    C8KEY_0,
    C8KEY_B,
    C8KEY_F,
    C8KEY_MAX
} C8KEY;

typedef struct {
    /* registers */
    uint8_t     v[VREGISTERS]; /* holds v0-vF */
    uint16_t    indx; /* memory index */
    uint16_t    sp; /* stack pointer */
    uint16_t    pc; /* program counter */
    uint8_t     dtimer; /* delay timer */
    uint8_t     stimer; /* sound timer */

    /* misc. memory */
    uint16_t     stack[STACKSIZE];
    uint8_t     ram[RAMSIZE];
    uint8_t     frame[VGFX][HGFX]; /* 64*32 1 bit frame buffer */
    int         keys[C8KEY_MAX];

    int ticks;
} C8State;

/* allocates a new chip8 emulator state */
C8State *vm_start();

/* loads the rom from the filename */
void vm_load(C8State *vm, const char* rom);

/* clears the frame buffer */
void vm_clear(C8State *vm);

/* emulates 1 tick */
int vm_tick(C8State *vm);

/* sets the key[k] state */
void vm_setKey(C8State *vm, C8KEY key, int state);

#endif