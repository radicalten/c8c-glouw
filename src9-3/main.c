//authored by Gemini-2.5-Pro input was: Help me write an SDL2 front end in C for my small Chip-8 emulator. here is my .h file:

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include "ch8.h" // Your Chip-8 core header

// --- Configuration ---
// Window dimensions (scaled from native Chip-8 resolution)
#define SCALE_FACTOR 15
#define WINDOW_WIDTH (CH8_DISPLAY_W * SCALE_FACTOR)
#define WINDOW_HEIGHT (CH8_DISPLAY_H * SCALE_FACTOR)

// Emulator clock speed in Hz
#define EMULATOR_HZ 700
#define TARGET_FPS 60
// Number of emulator cycles to run per frame to achieve the desired speed
#define CYCLES_PER_FRAME (EMULATOR_HZ / TARGET_FPS)

// --- Global SDL Variables ---
SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
SDL_Texture* g_texture = NULL;

// --- Function Prototypes ---
bool init_sdl(void);
void cleanup_sdl(void);
void handle_input(struct CH8_Core* ch8, bool* quit);
void update_graphics(const struct CH8_Core* ch8);
bool load_rom(struct CH8_Core* ch8, const char* filename);

// --- Main Function ---
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path/to/rom>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // 1. Initialize the Chip-8 Core
    struct CH8_Core ch8;
    if (!ch8_init(&ch8)) {
        fprintf(stderr, "Failed to initialize Chip-8 core.\n");
        return EXIT_FAILURE;
    }

    // 2. Load the ROM
    if (!load_rom(&ch8, argv[1])) {
        // Error message is printed inside load_rom
        return EXIT_FAILURE;
    }
    
    // 3. Initialize SDL
    if (!init_sdl()) {
        // Error message is printed inside init_sdl
        cleanup_sdl();
        return EXIT_FAILURE;
    }
    
    // 4. Main Emulation Loop
    bool quit = false;
    while (!quit) {
        // Handle keyboard and window events
        handle_input(&ch8, &quit);

        // Run multiple emulator cycles per frame to match target speed
        for (int i = 0; i < CYCLES_PER_FRAME; ++i) {
            ch8_run(&ch8);
        }

        // Update the screen if the draw flag has been set by an opcode
        if (ch8_should_draw(&ch8)) {
            update_graphics(&ch8);
        }
        
        // TODO: Handle sound when ch8_should_sound() is true
        if (ch8_should_sound(&ch8)) {
            // Beep! (Implement sound logic here)
        }

        // Cap the frame rate
        SDL_Delay(1000 / TARGET_FPS);
    }

    // 5. Cleanup and Exit
    cleanup_sdl();
    return EXIT_SUCCESS;
}

// --- Helper Functions ---

/**
 * @brief Initializes SDL, creates a window, renderer, and texture.
 * @return true on success, false on failure.
 */
bool init_sdl(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    g_window = SDL_CreateWindow("Chip-8 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!g_window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    // Create a texture that we will update with the Chip-8's pixel data
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                                  CH8_DISPLAY_W, CH8_DISPLAY_H);
    if (!g_texture) {
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

/**
 * @brief Destroys SDL objects and quits SDL.
 */
void cleanup_sdl(void) {
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
}

/**
 * @brief Maps standard keyboard keys to the Chip-8 keypad.
 *  CHIP-8      KEYBOARD
 *  1 2 3 C  ->  1 2 3 4
 *  4 5 6 D  ->  Q W E R
 *  7 8 9 E  ->  A S D F
 *  A 0 B F  ->  Z X C V
 */
void handle_input(struct CH8_Core* ch8, bool* quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            *quit = true;
        }

        // Handle key presses and releases
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            bool is_pressed = (e.type == SDL_KEYDOWN);
            CH8_Key key = -1; // Invalid key by default

            switch (e.key.keysym.sym) {
                case SDLK_1: key = CH8KEY_1; break;
                case SDLK_2: key = CH8KEY_2; break;
                case SDLK_3: key = CH8KEY_3; break;
                case SDLK_4: key = CH8KEY_C; break;

                case SDLK_q: key = CH8KEY_4; break;
                case SDLK_w: key = CH8KEY_5; break;
                case SDLK_e: key = CH8KEY_6; break;
                case SDLK_r: key = CH8KEY_D; break;

                case SDLK_a: key = CH8KEY_7; break;
                case SDLK_s: key = CH8KEY_8; break;
                case SDLK_d: key = CH8KEY_9; break;
                case SDLK_f: key = CH8KEY_E; break;

                case SDLK_z: key = CH8KEY_A; break;
                case SDLK_x: key = CH8KEY_0; break;
                case SDLK_c: key = CH8KEY_B; break;
                case SDLK_v: key = CH8KEY_F; break;
                
                case SDLK_ESCAPE: *quit = true; break;
            }

            if (key != -1) {
                ch8_set_key(ch8, key, is_pressed);
            }
        }
    }
}

/**
 * @brief Updates the SDL texture with the Chip-8's pixel data and renders it.
 */
void update_graphics(const struct CH8_Core* ch8) {
    // A temporary buffer to hold pixel data in the format SDL expects (RGBA)
    uint32_t pixels[CH8_DISPLAY_SIZE];

    for (int i = 0; i < CH8_DISPLAY_SIZE; ++i) {
        // Your core stores pixels as 0 or 1. We map this to colors.
        // On (1) -> White (0xFFFFFFFF), Off (0) -> Black (0xFF000000)
        pixels[i] = (ch8->display.pixels[i]) ? 0xFFFFFFFF : 0xFF000000;
    }

    // Update the texture with the new pixel data
    SDL_UpdateTexture(g_texture, NULL, pixels, CH8_DISPLAY_W * sizeof(uint32_t));

    // Clear the screen and render the texture (scaled up to window size)
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

/**
 * @brief Loads a ROM from a file into a buffer and then into the Chip-8 memory.
 * @param ch8 Pointer to the Chip-8 core.
 * @param filename Path to the ROM file.
 * @return true on success, false on failure.
 */
bool load_rom(struct CH8_Core* ch8, const char* filename) {
    FILE* rom_file = fopen(filename, "rb");
    if (!rom_file) {
        fprintf(stderr, "Error: Could not open ROM file '%s'\n", filename);
        return false;
    }

    // Get file size
    fseek(rom_file, 0, SEEK_END);
    long rom_size = ftell(rom_file);
    rewind(rom_file);

    if (rom_size > CH8_MAX_ROM_SIZE) {
        fprintf(stderr, "Error: ROM file size (%ld bytes) is too large for Chip-8 memory (%d bytes).\n",
                rom_size, CH8_MAX_ROM_SIZE);
        fclose(rom_file);
        return false;
    }

    // Allocate buffer and read ROM data
    uint8_t* rom_buffer = (uint8_t*)malloc(rom_size);
    if (!rom_buffer) {
        fprintf(stderr, "Error: Could not allocate memory for ROM buffer.\n");
        fclose(rom_file);
        return false;
    }

    size_t bytes_read = fread(rom_buffer, 1, rom_size, rom_file);
    if (bytes_read != rom_size) {
        fprintf(stderr, "Error: Could not read the full ROM file.\n");
        free(rom_buffer);
        fclose(rom_file);
        return false;
    }

    fclose(rom_file);

    // Load the ROM into the Chip-8 core's memory
    if (!ch8_loadrom(ch8, rom_buffer, rom_size)) {
        fprintf(stderr, "Error: ch8_loadrom failed.\n");
        free(rom_buffer);
        return false;
    }

    free(rom_buffer);
    printf("Successfully loaded ROM: %s (%ld bytes)\n", filename, rom_size);
    return true;
}
