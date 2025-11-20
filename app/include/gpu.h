#ifndef GPU_H
#define GPU_H

#include "memory.h"

#include <stdint.h>
#include <stdbool.h>

#define VRAM_BMAP_1 (0x9800 - MEM_VRAM_START)
#define VRAM_BMAP_2 (0x9C00 - MEM_VRAM_START)

#define VRAM_TILES_1 (0x8000 - MEM_VRAM_START)
#define VRAM_TILES_2 (0x8800 - MEM_VRAM_START)

#define LCD_MODE2_OAM_SCAN_DURATION     80
#define LCD_MODE3_LCD_DRAW_MIN_DURATION	172

#define LCD_MODE2_OAM_SCAN_END  (LCD_MODE2_OAM_SCAN_DURATION)
#define LCD_MODE3_LCD_DRAW_END  (LCD_MODE2_OAM_SCAN_END + LCD_MODE3_LCD_DRAW_MIN_DURATION)

void gpu_draw_line(struct gb_s *gb);

#endif