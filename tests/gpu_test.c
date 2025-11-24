#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "gb_types.h"
#include "cpu.h"
#include "memory.h"
#include "rom.h"

#define LCD_PALETTE_ALL 0x30

uint16_t fb[LCD_HEIGHT][LCD_WIDTH];
const uint32_t palette[] = { 0xFFFFFF, 0xA5A5A5, 0x525252, 0x000000 };

void lcd_draw_line(struct gb_s* gb, const uint8_t pixels[160], uint8_t line){
    for(unsigned int x = 0; x < LCD_WIDTH; x++) fb[line][x] = palette[pixels[x]];
    if(0){ gb->gb_frame = 0; } // placeholder
}

// #define TEST_ROM_FILE "../rom/fairylake.gb"
#define TEST_ROM_FILE "fairylake.gb"
#define TEST_DURATION 10

int main(void) {
    printf("====================================\n");
    printf("    Game Boy GPU Test\n");
    printf("====================================\n");

    uint16_t frames = 0;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // SDL3: SDL_CreateWindow(title, w, h, flags)
    // No more WINDOWPOS_CENTERED args
    SDL_Window *window = SDL_CreateWindow("gpu test",
                                          LCD_WIDTH*5, LCD_HEIGHT*5,
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // SDL3: SDL_CreateRenderer(window, name)
    // Use NULL for default driver, no flags arg
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Enable VSync via property
    SDL_SetRenderVSync(renderer, 1);

    // SDL3: SDL_PIXELFORMAT_XRGB1555 replaces SDL_PIXELFORMAT_RGB555
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_XRGB1555,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             LCD_WIDTH, LCD_HEIGHT);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    struct gb_s* gb = bootloader(TEST_ROM_FILE);
    if(!gb){
        fprintf(stderr, "Failed to load ROM\n");
    } else {
        gb->display.lcd_draw_line = lcd_draw_line;
        time_t start = time(NULL);
        while(time(NULL) - start < TEST_DURATION){
            gb->gb_frame = 0;
            while(!gb->gb_frame){ 
                cpu_step(gb); 
            }
            SDL_RenderClear(renderer);
            SDL_UpdateTexture(texture, NULL, fb, LCD_WIDTH * sizeof(uint16_t));
            SDL_FRect dst = {0, 0, LCD_WIDTH*5, LCD_HEIGHT*5};
            // SDL3: SDL_RenderTexture replaces SDL_RenderCopy
            SDL_RenderTexture(renderer, texture, NULL, &dst);
            SDL_RenderPresent(renderer);
            frames++;
        }
    }

    printf("total frames: %d\n", frames);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    free(gb);
    bootloader_cleanup();
    
    return 0;
}
