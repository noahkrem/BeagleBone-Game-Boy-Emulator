/**
 * Name: main.c
 * Description: Complete emulator integrating CPU, PPU, Memory, and Input.
 *              Based on gpu_test.c structure with added input and controls.
 * Author: Noah Kremler
 * 
 */

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gb_types.h"
#include "cpu.h"
#include "memory.h"
#include "rom.h"

/* Display scaling factor */
#define SCALE_FACTOR 5

/* Palette definition (same as Peanut-GB DMG colors) */
#define LCD_PALETTE_ALL 0x30

/* Frame buffer for LCD output */
static uint16_t fb[LCD_HEIGHT][LCD_WIDTH];

/* Color palette - DMG grayscale */
static const uint32_t palette[] = {
    0xFFFFFF,  /* White */
    0xA5A5A5,  /* Light gray */
    0x525252,  /* Dark gray */
    0x000000   /* Black */
};

/* Emulator state */
typedef struct {
    struct gb_s *gb;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    bool running;
    bool paused;
    uint32_t frame_count;
} emulator_state_t;

/**
 * LCD draw line callback - called by PPU for each scanline
 * This matches Peanut-GB's lcd_draw_line signature
 */
void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[160], uint8_t line) {
    (void)gb; /* Unused parameter */
    
    for (unsigned int x = 0; x < LCD_WIDTH; x++) {
        fb[line][x] = palette[pixels[x] & 0x03];
    }
}

/**
 * Handle SDL keyboard input and map to Game Boy controls
 */
void handle_input(emulator_state_t *emu, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT:
            emu->running = false;
            break;
            
        case SDL_EVENT_KEY_DOWN:
            switch (event->key.key) {
                /* Game Boy D-Pad */
                case SDLK_UP:
                    emu->gb->direct.joypad_bits.up = 0;
                    printf("DEBUG: UP pressed, joypad = 0x%02X\n", emu->gb->direct.joypad);
                    break;
                case SDLK_DOWN:
                    emu->gb->direct.joypad_bits.down = 0;
                    printf("DEBUG: DOWN pressed, joypad = 0x%02X\n", emu->gb->direct.joypad);
                    break;
                case SDLK_LEFT:
                    emu->gb->direct.joypad_bits.left = 0;
                    printf("DEBUG: LEFT pressed, joypad = 0x%02X\n", emu->gb->direct.joypad);
                    break;
                case SDLK_RIGHT:
                    emu->gb->direct.joypad_bits.right = 0;
                    printf("DEBUG: RIGHT pressed, joypad = 0x%02X\n", emu->gb->direct.joypad);
                    break;
                
                /* Game Boy Buttons */
                case SDLK_Z:  /* A button */
                    emu->gb->direct.joypad_bits.a = 0;
                    break;
                case SDLK_X:  /* B button */
                    emu->gb->direct.joypad_bits.b = 0;
                    break;
                case SDLK_RETURN:  /* Start */
                    emu->gb->direct.joypad_bits.start = 0;
                    break;
                case SDLK_RSHIFT:  /* Select */
                case SDLK_LSHIFT:
                    emu->gb->direct.joypad_bits.select = 0;
                    break;
                
                /* Emulator Controls */
                case SDLK_ESCAPE:
                    emu->running = false;
                    break;
                case SDLK_SPACE:
                    emu->paused = !emu->paused;
                    printf("%s\n", emu->paused ? "⏸  Paused" : "▶  Resumed");
                    break;
                case SDLK_R:
                    printf("🔄 Reset\n");
                    cpu_reset(emu->gb);
                    mmu_reset(emu->gb);
                    break;
                case SDLK_F:
                    printf("📊 Frames: %u\n", emu->frame_count);
                    break;
            }
            break;
            
        case SDL_EVENT_KEY_UP:
            switch (event->key.key) {
                /* Release D-Pad */
                case SDLK_UP:
                    emu->gb->direct.joypad_bits.up = 1;
                    break;
                case SDLK_DOWN:
                    emu->gb->direct.joypad_bits.down = 1;
                    break;
                case SDLK_LEFT:
                    emu->gb->direct.joypad_bits.left = 1;
                    break;
                case SDLK_RIGHT:
                    emu->gb->direct.joypad_bits.right = 1;
                    break;
                
                /* Release Buttons */
                case SDLK_Z:
                    emu->gb->direct.joypad_bits.a = 1;
                    break;
                case SDLK_X:
                    emu->gb->direct.joypad_bits.b = 1;
                    break;
                case SDLK_RETURN:
                    emu->gb->direct.joypad_bits.start = 1;
                    break;
                case SDLK_RSHIFT:
                case SDLK_LSHIFT:
                    emu->gb->direct.joypad_bits.select = 1;
                    break;
            }
            break;
    }
}

/**
 * Initialize SDL3 and create window/renderer
 */
bool init_sdl(emulator_state_t *emu) {
    /* Initialize SDL3 video subsystem */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    
    /* Create window */
    emu->window = SDL_CreateWindow(
        "Game Boy Emulator",
        LCD_WIDTH * SCALE_FACTOR,
        LCD_HEIGHT * SCALE_FACTOR,
        SDL_WINDOW_RESIZABLE
    );
    
    if (!emu->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    
    /* Create renderer */
    emu->renderer = SDL_CreateRenderer(emu->window, NULL);
    if (!emu->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(emu->window);
        SDL_Quit();
        return false;
    }
    
    /* Enable VSync for smooth 60 FPS */
    SDL_SetRenderVSync(emu->renderer, 1);
    
    /* Create texture for frame buffer */
    emu->texture = SDL_CreateTexture(
        emu->renderer,
        SDL_PIXELFORMAT_XRGB1555,
        SDL_TEXTUREACCESS_STREAMING,
        LCD_WIDTH,
        LCD_HEIGHT
    );
    
    if (!emu->texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(emu->renderer);
        SDL_DestroyWindow(emu->window);
        SDL_Quit();
        return false;
    }
    
    printf("✓ Display initialized (%dx%d, %dx scale)\n",
           LCD_WIDTH * SCALE_FACTOR, LCD_HEIGHT * SCALE_FACTOR, SCALE_FACTOR);
    
    return true;
}

/**
 * Cleanup SDL resources
 */
void cleanup_sdl(emulator_state_t *emu) {
    if (emu->texture) {
        SDL_DestroyTexture(emu->texture);
    }
    if (emu->renderer) {
        SDL_DestroyRenderer(emu->renderer);
    }
    if (emu->window) {
        SDL_DestroyWindow(emu->window);
    }
    SDL_Quit();
}

/**
 * Run one frame of emulation
 */
void run_frame(emulator_state_t *emu) {
    /* Reset frame flag */
    emu->gb->gb_frame = 0;
    
    /* Execute CPU until frame is complete */
    while (!emu->gb->gb_frame) {
        cpu_step(emu->gb);
    }
    
    emu->frame_count++;
}

/**
 * Update display with current frame buffer
 */
void update_display(emulator_state_t *emu) {
    /* Clear renderer */
    SDL_RenderClear(emu->renderer);
    
    /* Update texture with frame buffer */
    SDL_UpdateTexture(emu->texture, NULL, fb, LCD_WIDTH * sizeof(uint16_t));
    
    /* Render texture scaled to window size */
    SDL_FRect dst = {0, 0, LCD_WIDTH * SCALE_FACTOR, LCD_HEIGHT * SCALE_FACTOR};
    SDL_RenderTexture(emu->renderer, emu->texture, NULL, &dst);
    
    /* Present to screen */
    SDL_RenderPresent(emu->renderer);
}

/**
 * Main emulation loop
 */
void emulator_loop(emulator_state_t *emu) {
    SDL_Event event;
    
    printf("\nStarting emulation...\n");
    printf("Controls:\n");
    printf("  Arrow Keys = D-Pad\n");
    printf("  Z = A Button\n");
    printf("  X = B Button\n");
    printf("  Enter = Start\n");
    printf("  Shift = Select\n");
    printf("  Space = Pause\n");
    printf("  R = Reset\n");
    printf("  F = Show frame count\n");
    printf("  ESC = Quit\n\n");
    
    while (emu->running) {
        /* Handle all pending events */
        while (SDL_PollEvent(&event)) {
            handle_input(emu, &event);
        }
        
        /* Run emulation if not paused */
        if (!emu->paused) {
            run_frame(emu);
            update_display(emu);
        }
        
        /* Small delay if paused to reduce CPU usage */
        if (emu->paused) {
            SDL_Delay(16);  /* ~60 FPS */
        }
    }
    
    printf("\nTotal frames rendered: %u\n", emu->frame_count);
}

/**
 * Main entry point
 */
int main(int argc, char **argv) {
    printf("====================================\n");
    printf("    Game Boy Emulator\n");
    printf("====================================\n\n");
    
    /* Check command line arguments */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom_file.gb>\n", argv[0]);
        return 1;
    }
    
    char *rom_path = argv[1];
    
    /* Initialize emulator state */
    emulator_state_t emu = {0};
    emu.running = true;
    emu.paused = false;
    emu.frame_count = 0;
    
    /* Initialize SDL */
    if (!init_sdl(&emu)) {
        return 1;
    }
    
    /* Load ROM via bootloader */
    printf("Loading ROM: %s\n", rom_path);
    emu.gb = bootloader(rom_path);
    
    if (!emu.gb) {
        fprintf(stderr, "Failed to load ROM: %s\n", rom_path);
        cleanup_sdl(&emu);
        return 1;
    }
    
    /* Set up LCD draw callback */
    emu.gb->display.lcd_draw_line = lcd_draw_line;
    
    /* Initialize joypad to "all buttons released" state */
    emu.gb->direct.joypad = 0xFF;

    // Initialize frame debug counter
    emu.gb->frame_debug = 0;
    
    printf("✓ ROM loaded successfully\n");
    
    /* Run main emulation loop */
    emulator_loop(&emu);
    
    /* Cleanup */
    printf("\nCleaning up...\n");
    free(emu.gb);
    bootloader_cleanup();
    cleanup_sdl(&emu);
    
    printf("✓ Cleanup complete\n");
    printf("\nGoodbye!\n");
    
    return 0;
}