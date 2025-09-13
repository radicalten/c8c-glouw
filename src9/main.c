#include <stdio.h>
#include <windows.h>
#include <SDL2/SDL.h>
#include "main.h"
#include "cpu.h"

#define WIDTH 128
#define HEIGHT 64

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

bool window_create() {
    uint8_t pixel_scale = 16;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        printf("Could not init SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    
    window = SDL_CreateWindow("Chip-8 Intepreter",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WIDTH * (pixel_scale / 2),
                              HEIGHT * (pixel_scale / 2),
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    assert(window != NULL);
    assert(renderer != NULL);
    SDL_RenderSetScale(renderer, pixel_scale, pixel_scale);
    
    return true;
}

void window_process_input(cpu_t *cpu) {
    SDL_Event e;
    if (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
            cpu->is_running = false;
            break;
            
            case SDL_KEYDOWN:
            switch(e.key.keysym.sym) {
                case SDLK_ESCAPE:
                cpu->is_running = false;
                break;
                case SDLK_1:
                cpu->key[0x0001] = 1;
                break;
                case SDLK_2:
                cpu->key[0x0002] = 1;
                break;
                case SDLK_3:
                cpu->key[0x0003] = 1;
                break;
                case SDLK_4:
                cpu->key[0x000C] = 1;
                break;
                case SDLK_q:
                cpu->key[0x0004] = 1;
                break;
                case SDLK_w:
                cpu->key[0x0005] = 1;
                break;
                case SDLK_e:
                cpu->key[0x0006] = 1;
                break;
                case SDLK_r:
                cpu->key[0x000D] = 1;
                break;
                case SDLK_a:
                cpu->key[0x0007] = 1;
                break;
                case SDLK_s:
                cpu->key[0x0008] = 1;
                break;
                case SDLK_d:
                cpu->key[0x0009] = 1;
                break;
                case SDLK_f:
                cpu->key[0x000E] = 1;
                break;
                case SDLK_z:
                cpu->key[0x000A] = 1;
                break;
                case SDLK_x:
                cpu->key[0x0000] = 1;
                break;
                case SDLK_c:
                cpu->key[0x000B] = 1;
                break;
                case SDLK_v:
                cpu->key[0x000F] = 1;
                break;        
            }
            break;
            case SDL_KEYUP:
            switch(e.key.keysym.sym) {
                case SDLK_1:
                cpu->key[0x0001] = 0;
                break;
                case SDLK_2:
                cpu->key[0x0002] = 0;
                break;
                case SDLK_3:
                cpu->key[0x0003] = 0;
                break;
                case SDLK_4:
                cpu->key[0x000C] = 0;
                break;
                case SDLK_q:
                cpu->key[0x0004] = 0;
                break;
                case SDLK_w:
                cpu->key[0x0005] = 0;
                break;
                case SDLK_e:
                cpu->key[0x0006] = 0;
                break;
                case SDLK_r:
                cpu->key[0x000D] = 0;
                break;
                case SDLK_a:
                cpu->key[0x0007] = 0;
                break;
                case SDLK_s:
                cpu->key[0x0008] = 0;
                break;
                case SDLK_d:
                cpu->key[0x0009] = 0;
                break;
                case SDLK_f:
                cpu->key[0x000E] = 0;
                break;
                case SDLK_z:
                cpu->key[0x000A] = 0;
                break;
                case SDLK_x:
                cpu->key[0x0000] = 0;
                break;
                case SDLK_c:
                cpu->key[0x000B] = 0;
                break;
                case SDLK_v:
                cpu->key[0x000F] = 0;
                break;        
            }
            break;
        }
    }
}

void window_render(cpu_t *cpu) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
    for (int i = 0; i < DISPLAY_HEIGHT; i++) {
        for (int j = 0; j < DISPLAY_WIDTH; j++) {
            // draw if bit is active
            if (cpu->display[j + (i * DISPLAY_WIDTH)] != 0) {
                SDL_RenderDrawPoint(renderer, j, i);
            }
        }
    }
    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    // checking if a ROM file was specificed
    if (argc < 2 || argc > 2) {
        printf("Usage: ./chip8.out <ROM>\n");
        return -1;
    }
    srand(time(NULL));
    
    cpu_t cpu = cpu_create();
    // load rom into memory
    cpu_load_rom(&cpu, argv[1]);
    
    window_create();
    
    while (cpu.is_running) {
        window_process_input(&cpu);
        cpu_decode_execute(&cpu);
        if (cpu.should_draw) {
            window_render(&cpu);
        }
        
        SDL_Delay(2);
    }
    
    //cpu_print_status(&cpu);
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
