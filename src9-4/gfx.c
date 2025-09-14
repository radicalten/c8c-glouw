#include "gfx.h"

SDL_Window *win;
SDL_Renderer *rend;
int closed = 0;

void renderer_init() {
    if (SDL_Init(SDL_INIT_EVERYTHING)) {
        printf("error initializing SDL: %s\n", SDL_GetError());
    }

    /* create window & renderer */
    win = SDL_CreateWindow("C8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    rend =  SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
}

void renderer_close() {
    SDL_DestroyWindow(win);
    SDL_Quit();
}

void renderer_refresh(C8State *vm) {
    int row, col;
    SDL_Rect pixel;
    pixel.w = PIXEL_SIZE;
    pixel.h = PIXEL_SIZE;

    /* sets the background to black */
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);

    /* clears the window */
    SDL_RenderClear(rend);

    /* sets the pixel color to white */
    SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);

    /* walk through each row and column and draw each pixel */
    for (row = 0; row < VGFX; row++) {
        for (col = 0; col < HGFX; col++) {
            if (vm->frame[row][col]) {
                /* pixel is alive, draw it */
                pixel.x = col * PIXEL_SIZE;
                pixel.y = row * PIXEL_SIZE;
                SDL_RenderFillRect(rend, &pixel);
            }
        }
    }

    /* render everything to the screen */
    SDL_RenderPresent(rend);
}

C8KEY getKey(SDL_Scancode c) {
    switch (c) {
        case SDL_SCANCODE_1: return C8KEY_1;
        case SDL_SCANCODE_2: return C8KEY_2;
        case SDL_SCANCODE_3: return C8KEY_3;
        case SDL_SCANCODE_4: return C8KEY_C;
        case SDL_SCANCODE_Q: return C8KEY_4;
        case SDL_SCANCODE_W: return C8KEY_5;
        case SDL_SCANCODE_E: return C8KEY_6;
        case SDL_SCANCODE_R: return C8KEY_D;
        case SDL_SCANCODE_A: return C8KEY_7;
        case SDL_SCANCODE_S: return C8KEY_8;
        case SDL_SCANCODE_D: return C8KEY_9;
        case SDL_SCANCODE_F: return C8KEY_E;
        case SDL_SCANCODE_Z: return C8KEY_A;
        case SDL_SCANCODE_X: return C8KEY_0;
        case SDL_SCANCODE_C: return C8KEY_B;
        case SDL_SCANCODE_V: return C8KEY_F;
        default:
            return C8KEY_MAX; /* error return */
    }
}

C8KEY pollEvents(C8State *vm) {
    SDL_Event event;
    C8KEY key = C8KEY_MAX;

    /* manage queued events */
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                closed = 1;
                break;
            case SDL_KEYDOWN:
                vm_setKey(vm, key = getKey(event.key.keysym.scancode), 1);
                break;
            case SDL_KEYUP:
                vm_setKey(vm, key = getKey(event.key.keysym.scancode), 0);
                break;
            default: break;
        }
    }

    SDL_Delay(CLOCK_RATE);

    return key;
}

int renderer_waitforinput(C8State *vm) {
    C8KEY key;

    /* keep polling until we get a key or the window closes */
    while ((key = pollEvents(vm)) == C8KEY_MAX && !closed);
    return key;
}

int renderer_step(C8State *vm) {
    /* tick the vm */
    vm_tick(vm);
    pollEvents(vm);

    return !closed;
}