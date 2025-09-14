// authored by GPT5-chat. prompt Help me write an SDL2 front end in C for my small Chip-8 emulator. here is my .h file:

#include <SDL2/SDL.h>
#include <stdio.h>
#include "ch8.h"   // Your header file

#define WINDOW_SCALE 10  // Scale factor (64x32 → 640x320)

// Map SDL keys to Chip-8 keys.
static int map_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_1: return CH8KEY_1;
        case SDLK_2: return CH8KEY_2;
        case SDLK_3: return CH8KEY_3;
        case SDLK_4: return CH8KEY_C;
        case SDLK_q: return CH8KEY_4;
        case SDLK_w: return CH8KEY_5;
        case SDLK_e: return CH8KEY_6;
        case SDLK_r: return CH8KEY_D;
        case SDLK_a: return CH8KEY_7;
        case SDLK_s: return CH8KEY_8;
        case SDLK_d: return CH8KEY_9;
        case SDLK_f: return CH8KEY_E;
        case SDLK_z: return CH8KEY_A;
        case SDLK_x: return CH8KEY_0;
        case SDLK_c: return CH8KEY_B;
        case SDLK_v: return CH8KEY_F;
        default: return -1;
    }
}

// Simple "beep" callback for SDL audio.
static void audio_callback(void* userdata, Uint8* stream, int len) {
    static int phase = 0;
    for (int i = 0; i < len; i++, phase++) {
        stream[i] = (phase / 100) % 2 ? 128 : 255; // crude square wave
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s romfile\n", argv[0]);
        return 1;
    }

    // Load ROM file into memory
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        perror("ROM open failed");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    uint8_t* rom = malloc(size);
    fread(rom, 1, size, f);
    fclose(f);

    // Initialize Chip-8
    struct CH8_Core chip8;
    if (!ch8_init(&chip8)) {
        fprintf(stderr, "Failed to init Chip-8\n");
        return 1;
    }
    if (!ch8_loadrom(&chip8, rom, (uint32_t)size)) {
        fprintf(stderr, "Failed to load ROM\n");
        return 1;
    }
    free(rom);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Chip-8 Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        CH8_DISPLAY_W * WINDOW_SCALE, CH8_DISPLAY_H * WINDOW_SCALE,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Create surface texture (monochrome screen)
    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        CH8_DISPLAY_W, CH8_DISPLAY_H
    );

    // Setup audio
    SDL_AudioSpec want = {0};
    want.freq = 44100;
    want.format = AUDIO_U8;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audio_callback;

    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    SDL_PauseAudioDevice(audio_dev, 1); // start paused

    // Main loop
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                int mapped = map_key(e.key.keysym.sym);
                if (mapped >= 0) {
                    ch8_set_key(&chip8, (CH8_Key)mapped,
                        (e.type == SDL_KEYDOWN));
                }
            }
        }

        // Run emulation cycle
        ch8_run(&chip8);

        if (ch8_should_draw(&chip8)) {
            uint32_t* pixels;
            int pitch;
            SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);
            for (int y = 0; y < CH8_DISPLAY_H; y++) {
                for (int x = 0; x < CH8_DISPLAY_W; x++) {
                    bool on = ch8_get_pixel(&chip8, x, y);
                    pixels[y * CH8_DISPLAY_W + x] = on ? 0xFFFFFFFF : 0xFF000000;
                }
            }
            SDL_UnlockTexture(texture);

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        // Handle sound
        if (ch8_should_sound(&chip8)) {
            SDL_PauseAudioDevice(audio_dev, 0); // play tone
        } else {
            SDL_PauseAudioDevice(audio_dev, 1); // silence
        }

        SDL_Delay(2); // crude pacing; you may want timer-based sync
    }

    // Cleanup
    SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
