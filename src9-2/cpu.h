#ifndef CPU_H
#define CPU_H

typedef unsigned char uint8;
typedef signed char int8;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned long uint32;
typedef signed long int32;

typedef int bool;
#define TRUE 1
#define FALSE 0

#define RAM_SIZE 4096
#define START_ADDRESS 0x200
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32

typedef struct {
  uint16 opcode;
  uint8 t;
  uint8 x;
  uint8 y;
  uint8 n;
  uint8 nn;
  uint16 nnn;
} nibble_t;

typedef struct {
  uint8 registers[16];
  uint16 i_register;
  uint8 memory[RAM_SIZE];
  uint16 stack[16];
  uint8 keypad[16];
  bool video[SCREEN_WIDTH * SCREEN_HEIGHT];
  uint16 program_counter;
  uint8 stack_pointer;
  nibble_t opcode;
  uint8 delay_timer;
  uint8 sound_timer;
  bool draw_flag;
} cpu_t;

/* Init CPU */
void reset(cpu_t *cpu);
int load_rom(cpu_t *cpu, const char *path);

/* CPU */
void execute(cpu_t *cpu, nibble_t op);
void tick(cpu_t *cpu);
nibble_t _decode(uint16 opcode);
nibble_t _fetch(cpu_t *cpu);

/* Instructions */
void _add(cpu_t *cpu, nibble_t op);
void _add_with_carry(cpu_t *cpu, nibble_t op);
void _sub_with_carry(cpu_t *cpu, nibble_t op);
void _shr(cpu_t *cpu, nibble_t op);
void _subn_with_carry(cpu_t *cpu, nibble_t op);
void _shl(cpu_t *cpu, nibble_t op);
void _rnd(cpu_t *cpu, nibble_t op);
void _drw(cpu_t *cpu, nibble_t op);
void _bcd(cpu_t *cpu, nibble_t op);
void _str_v_to_mem(cpu_t *cpu, nibble_t op);
void _ld_mem_to_v(cpu_t *cpu, nibble_t op);

/* Stack*/
void push_stack(cpu_t *cpu, uint16 value);
uint16 pop_stack(cpu_t *cpu);

/* Timers */
void tick_timers(cpu_t *cpu);

/* Keyboard */
void push_key(cpu_t *cpu, uint8 key);
void release_key(cpu_t *cpu, uint8 key);
void key_press(cpu_t *cpu, uint8 key, bool is_pressed);
void chk_key_pressed(cpu_t *cpu, nibble_t opcode);

#endif