#include "chip.h"
#include "tui.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

#define vm_fatal(format, ...) \
    printf("[FATAL] " format, __VA_ARGS__); \
    exit(1);

uint8_t C8FONT[] = { 
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
    0xF0, 0x80, 0xF0, 0x80, 0x80  /* F */ 
};

C8State *vm_start() {
    C8State *vm = malloc(sizeof(C8State));
    int i;

    if (vm == NULL) {
        vm_fatal("Failed to malloc()! errno [%d]\n", errno);
    }

    vm->sp = 0;
    vm->pc = STARTPC;
    vm->indx = 0;
    vm->dtimer = 0;
    vm->stimer = 0;
    vm->ticks = 0;

    /* zero out the stack, registers & memory */
    for (i = 0; i < STACKSIZE; i++)
        vm->stack[i] = 0;

    for (i = 0; i < RAMSIZE; i++)
        vm->ram[i] = 0;

    for (i = 0; i < VREGISTERS; i++) 
        vm->v[i] = 0;
    
    /* also zero out the key states */
    for (i = 0; i < C8KEY_MAX; i++)
        vm->keys[i] = 0;

    /* load the font into memory */
    for (i = 0; i < (sizeof(C8FONT) / sizeof(uint8_t)); i++)
        vm->ram[FONTADDR + i] = C8FONT[i];

    /* zero out the frame buffer */ 
    vm_clear(vm);
    return vm;
}

void vm_load(C8State *vm, const char* rom) {
    FILE *file = fopen(rom, "rb");

    if (file == NULL) {
        vm_fatal("Unable to open rom '%s'", rom);
    }

    /* load rom into ram at our start address */
    fread(&vm->ram[STARTPC], 1, RAMSIZE - STARTPC, file);
    fclose(file);
}

void vm_clear(C8State *vm) {
    int y, x;

    for (y = 0; y < VGFX; y++) {
        for (x = 0; x < HGFX; x++) {
            vm->frame[y][x] = 0;
        }
    }
}

void vm_setKey(C8State *vm, C8KEY key, int state) {
    if (key != C8KEY_MAX)
        vm->keys[key] = state;
}

/* draws the sprite n at (x, y) */
void vm_draw(C8State *vm, int x, int y, int n) {
    unsigned byteI, bitI;
    uint8_t bit;
    uint8_t *pixel;

    /* for n bytes at ram[i], draw that sprite to the screen */
    for (byteI = 0; byteI < n; byteI++) {
        for (bitI = 0; bitI < 8; bitI++) {
            bit = (vm->ram[vm->indx + byteI] >> bitI) & 0x1;

            /* grab a pointer to the pixel */
            pixel = &vm->frame[(y + byteI) % VGFX][(x + (7 - bitI)) % HGFX];

            /* if the pixel is going to be erased and it's on, set the collision flag */
            if (bit & *pixel) 
                vm->v[0xF] = 1;

            /* draw to the buffer */
            *pixel = *pixel ^ bit; 
        }
    }

    renderer_refresh(vm);
}

void vm_unimpl(C8State *vm, uint16_t i) {
    vm_fatal("Unknown instruction at 0x%04X [0x%04X]\n", vm->pc - 2, i);
}

/*
        chip8's instruction set is very weird, the opcodes are all over the place, which is why this switch
    statement has so many branches. i tried to group things together as much as possible though
*/

int vm_tick(C8State *vm) {
    int i;

    /* grab the current instruction and increment the program counter */
    uint16_t instr = vm->ram[vm->pc] << 8 | vm->ram[vm->pc + 1];
    vm->pc += 2;

    if (vm->ticks++ % 60) { /* every 60 ticks, decrement timers */
        if (vm->dtimer > 0)
            vm->dtimer--;
        
        if (vm->stimer > 0)
            vm->stimer--;
    }
    
    /* grab the opcode */
    switch (GET_X000(instr)) {
    case 0x0: { /* basic instructions */
        /* grab 2nd opcode (i'm ignoring the SYS instruction on purpose, no one implements it anyways) */
        switch (GET_00XX(instr)) {
        case 0xE0: /* CLEAR */
            vm_clear(vm);
            break;
        case 0xEE: /* RET */
            /* pop address off the stack and set pc to it */
            vm->pc = vm->stack[--vm->sp];
            break;
        default:
            vm_unimpl(vm, instr);
        }
        break;
    }
    case 0x1: /* JMP addr */
        vm->pc = GET_0XXX(instr);
        break;
    case 0x2: /* CALL addr */
        /* push program counter to the stack and jump to addr */
        vm->stack[vm->sp++] = vm->pc;
        vm->pc = GET_0XXX(instr);
        break;
    case 0x3: /* SE vx, byte */
        /* if v[x] is equal to byte, skip the next instruction */
        if (vm->v[GET_0X00(instr)] == GET_00XX(instr))
            vm->pc += 2;
        break;
    case 0x4: /* SNE vx, byte */
        /* if v[x] is NOT equal to byte, skip the next instruction */
        if (vm->v[GET_0X00(instr)] != GET_00XX(instr))
            vm->pc += 2;
        break;
    case 0x5: /* SE vx, vy */
        /* if v[x] is equal to v[y], skip the next instruction */
        if (vm->v[GET_0X00(instr)] == vm->v[GET_00X0(instr)])
            vm->pc += 2;
        break;
    case 0x6: /* LD vx, byte */
        /* load byte into v[x] */
        vm->v[GET_0X00(instr)] = GET_00XX(instr);
        break;
    case 0x7: /* ADD vx, byte */
        /* adds byte to v[x] and stores it in v[x] */
        vm->v[GET_0X00(instr)] += GET_00XX(instr);
        break;
    case 0x8: /* generic bitwise operations on v registers */
        /* grab 2nd opcode */
        switch(GET_000X(instr)) {
        case 0x0: /* LD vx, vy */
            /* sets v[x] to v[y] */
            vm->v[GET_0X00(instr)] = vm->v[GET_00X0(instr)];
            break;
        case 0x1: /* OR vx, vy */
            /* ORs v[x]  and v[y] together, stores the result in v[x] */
            vm->v[GET_0X00(instr)] |= vm->v[GET_00X0(instr)];
            break;
        case 0x2: /* AND vx, vy */
            /* ANDs v[x]  and v[y] together, stores the result in v[x] */
            vm->v[GET_0X00(instr)] &= vm->v[GET_00X0(instr)];
            break;
        case 0x3: /* XOR vx, vy */
            /* XORs v[x]  and v[y] together, stores the result in v[x] */
            vm->v[GET_0X00(instr)] ^= vm->v[GET_00X0(instr)];
            break;
        case 0x4: /* ADD vx, vy */
            /* ADDs v[x] and v[y] together, stores the result in v[x]. if there's an overflow, set the carry bit in v[0xF] */
            vm->v[0xF] = ((int)vm->v[GET_0X00(instr)]) + ((int)vm->v[GET_00X0(instr)]) > 255 ? 1 : 0;
            vm->v[GET_0X00(instr)] += vm->v[GET_00X0(instr)];
            break;
        case 0x5: /* SUB vx, vy */
            /* SUBs v[y] from v[x] and stores the result in v[x]. if there's an underflow, set the NOT borrow bit in v[0xF] */
            vm->v[0xF] = vm->v[GET_0X00(instr)] > vm->v[GET_00X0(instr)]; /* vx > vy, set v[0xF] to 1 */
            vm->v[GET_0X00(instr)] -= vm->v[GET_00X0(instr)];
            break;
        case 0x6: /* SHR vx */
            /* copy the least-significant bit of v[x] to v[0xF], then bit shift v[x] by 1 to the right and store the result in v[x] */
            vm->v[0xF] = vm->v[GET_0X00(instr)] & 0x01;
            vm->v[GET_0X00(instr)] >>= 1;
            break;
        case 0x7: /* SUBN vx, vy */
            /* SUBs v[x] from v[y] and stores the result in v[x]. if there's an underflow, set the NOT borrow bit in v[0xF] */
            vm->v[0xF] = vm->v[GET_00X0(instr)] > vm->v[GET_0X00(instr)]; /* vy > vx, set v[0xF] to 1 */
            vm->v[GET_0X00(instr)] = vm->v[GET_00X0(instr)] - vm->v[GET_0X00(instr)];
            break;
        case 0xE: /* SHL vx, vy */
            /* copy the most-significant bit of v[x] to v[0xF], then bit shift v[x] by 1 to the left and store the result in v[x] */
            vm->v[0xF] = (vm->v[GET_0X00(instr)] & 0x80) != 0;
            vm->v[GET_0X00(instr)] <<= 1;
            break;
        default:
            vm_unimpl(vm, instr);
        }
        break;
    case 0x9: /* SNE vx, vy */
        /* if v[x] != v[y] skip the next instruction */
        if (vm->v[GET_0X00(instr)] != vm->v[GET_00X0(instr)])
            vm->pc += 2;
        break;
    case 0xA: /* LD I, addr */
        /* load address into index register */
        vm->indx = GET_0XXX(instr);
        break;
    case 0xB: /* JP v0, addr */
        /* jump to addr + v0 */
        vm->pc = GET_0XXX(instr) + vm->v[0];
        break;
    case 0xC: /* RND vx, byte */
        /* generates a random number (0-255) and ANDs it with byte, the result is stored in v[x] */
        vm->v[GET_0X00(instr)] = (rand() % 256) & GET_00XX(instr);
        break;
    case 0xD: /* DRW vx, vy, n */
        /* draw v[x], v[y], n */
        vm_draw(vm, vm->v[GET_0X00(instr)], vm->v[GET_00X0(instr)], GET_000X(instr));
        break;
    case 0xE: { /* key-pressing instructions */
        /* grab 2nd opcode */
        switch (GET_00XX(instr)) {
        case 0x9E: /* SKP vx */
            /* if keys[v[x]] is pressed, skip the next instruction */
            if (vm->keys[GET_0X00(instr)])
                vm->pc += 2;
            break;
        case 0xA1: /* SNKP vx */
            /* if keys[v[x]] is not pressed, skip the next instruction */
            if (!vm->keys[GET_0X00(instr)])
                vm->pc += 2;
            break;
        default:
            vm_unimpl(vm, instr);
        }
        break;
    }
    case 0xF: {
        /* grab 2nd opcode */
        switch(GET_00XX(instr)) {
        case 0x07: /* LD vx, dt */
            /* load dt into v[x] */
            vm->v[GET_0X00(instr)] = vm->dtimer;
            break;
        case 0x0A: /* LD vx, k */
            /* wait for a keypress, store key in v[x] */
            vm->v[GET_0X00(instr)] = renderer_waitforinput(vm);
            break;
        case 0x15: /* LD dt, vx */
            /* load v[x] into delay timer */
            vm->dtimer = vm->v[GET_0X00(instr)];
            break;
        case 0x18: /* LD st, vx */
            /* load v[x] into sound timer */
            vm->stimer = vm->v[GET_0X00(instr)];
            break;
        case 0x1E: /* ADD I, vx */
            /* add v[x] to indx, store back in indx */
            vm->v[0xF] = (vm->v[GET_0X00(instr)] + vm->indx) > 0xFFF ? 1 : 0;
            vm->indx = vm->v[GET_0X00(instr)] + vm->indx;
            break;
        case 0x29: /* LD F, vx */
            /* load the address of sprite font id v[x] in memory to I */
            vm->indx = FONTADDR + (vm->v[GET_0X00(instr)] * 5); /* 5 bytes per character */
            break;
        case 0x33: /* LD B, vx */
            /* load BCD of v[x] to I, I+1, and I+2 */
            vm->ram[vm->indx]       = (vm->v[GET_0X00(instr)] / 100) % 10; /* hundreds digit */
            vm->ram[vm->indx + 1]   = (vm->v[GET_0X00(instr)] / 10) % 10; /* tens digit */
            vm->ram[vm->indx + 2]   = (vm->v[GET_0X00(instr)] % 10); /* ones digit  */
            break;
        case 0x55: /* LD [I], vx */
            /* store v[0x0] through v[0x8] to memory starting at I. move I to the end */
            for (i = 0; i <= GET_0X00(instr); i++)
                vm->ram[vm->indx + i] = vm->v[i];
            vm->indx += GET_0X00(instr) + 1;
            break;
        case 0x65: /* LD vx, [I] */
            /* copies ram into the v[0] through v[x] registers starting at I. move I to the end */
            for (i = 0; i <= GET_0X00(instr); i++)
                vm->v[i] = vm->ram[vm->indx + i];
            vm->indx += GET_0X00(instr) + 1;
            break;
        default:
            vm_unimpl(vm, instr);
        }
        break;
    }
    default:
        vm_unimpl(vm, instr);
    }

    return 0;
}