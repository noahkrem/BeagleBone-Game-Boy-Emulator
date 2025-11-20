/**
 * gpu_test.c
 */

#include <SDL2/SDL.h>
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



#define TEST_ROM_FILE "../rom/fairylake.gb"
#define TEST_DURATION 10

int main(void) {
    
    
    printf("====================================\n");
    printf("    Game Boy GPU Test\n");
    printf("====================================\n");

    uint16_t frames = 0;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    window = SDL_CreateWindow("gpu test",
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                LCD_WIDTH*5, LCD_HEIGHT*5,
                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_INPUT_FOCUS);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_RGB555,
                                SDL_TEXTUREACCESS_STREAMING,
                                LCD_WIDTH, LCD_HEIGHT);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // nearest neighbor

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
                // usleep(100/4);
            }
            SDL_RenderClear(renderer);
            SDL_UpdateTexture(texture, NULL, fb, LCD_WIDTH * sizeof(uint16_t));
            SDL_Rect dst = {0, 0, LCD_WIDTH*5, LCD_HEIGHT*5};
            SDL_RenderCopy(renderer, texture, NULL, &dst);
            SDL_RenderPresent(renderer);
            frames++;
        }

    }

    printf("total frames: %d\n", frames);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_DestroyTexture(texture);
    SDL_Quit();
    
    free(gb);
    bootloader_cleanup();
    
    return 0;
}