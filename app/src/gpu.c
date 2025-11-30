#include "gpu.h"
#include "gb_types.h"

#include <stdio.h>
#include <stdlib.h>

void gpu_draw_line(struct gb_s *gb){

	if (gb->frame_debug < 5 && gb->hram_io[IO_LY] == 0) {
		printf("DEBUG FRAME %u: LCDC=0x%02X LY=%u mode=%u\n",
			gb->frame_debug,
			gb->hram_io[IO_LCDC],
			gb->hram_io[IO_LY],
			gb->hram_io[IO_STAT] & 0x03);
	}

	// Per-line buffer (2‑bit color indices 0–3)
	uint8_t pixels[160] = {0};

	/* If LCD not initialised by front-end, don't render anything. */
    if(gb->display.lcd_draw_line == NULL) return;

    /* Render unless LCD is completely disabled (0x00) */
	if (gb->hram_io[IO_LCDC] == 0x00) return;

    // DEBUG: Check if ANY tile data exists in VRAM
	if (gb->frame_debug < 2 && gb->hram_io[IO_LY] == 0) {
		int non_zero_tiles = 0;
		for (int i = 0; i < 0x1800; i++) {  // Check tile data area
			if (gb->vram[i] != 0) non_zero_tiles++;
		}
		printf("DEBUG: Non-zero VRAM bytes in tile area: %d / %d\n", 
			non_zero_tiles, 0x1800);
		
		// Check tilemap
		int non_zero_map = 0;
		for (int i = 0x1800; i < 0x1C00; i++) {  // Tilemap 0
			if (gb->vram[i] != 0) non_zero_map++;
		}
		printf("DEBUG: Non-zero tilemap entries: %d / 1024\n", non_zero_map);
	}

	/* If LCD not initialised by front-end, don't render anything. */
	if(gb->display.lcd_draw_line == NULL) return;

	// DEBUG: print palette at start of frame
    static int frame_count = 0;
    if (frame_count == 0 && gb->hram_io[IO_LY] == 0) {
        printf("PALETTE: BGP=0x%02X bg[0..3]=%u,%u,%u,%u\n",
               gb->hram_io[IO_BGP],
               gb->display.bg_palette[0],
               gb->display.bg_palette[1],
               gb->display.bg_palette[2],
               gb->display.bg_palette[3]);
        frame_count++;
    }

	// DEBUG: force visible output for this line
    // for (int i = 0; i < LCD_WIDTH; i++) {
    //     pixels[i] = gb->display.bg_palette[3];  // or just 3 if you want raw index
    // }

	/* If background is enabled, draw it. */
	if(gb->hram_io[IO_LCDC] & LCDC_BG_ENABLE){
		uint8_t bg_y, disp_x, bg_x, idx, py, px, t1, t2;
		uint16_t bg_map, tile;

		/* 
		 * Calculate current background line to draw. Constant because
		 * this function draws only this one line each time it is
		 * called. 
		 */
		bg_y = gb->hram_io[IO_LY] + gb->hram_io[IO_SCY];

		/* 
         * Get selected background map address for first tile
		 * corresponding to current line.
		 * 0x20 (32) is the width of a background tile, and the bit
		 * shift is to calculate the address.
         */
		bg_map = ((gb->hram_io[IO_LCDC] & LCDC_BG_MAP) ? VRAM_BMAP_2 : VRAM_BMAP_1) + (bg_y >> 3) * 0x20;

		// Debugging
		if (gb->frame_debug < 3 && (gb->hram_io[IO_LY] == 0 || gb->hram_io[IO_LY] == 80)) {
			uint32_t sum = 0;
			for (int x = 0; x < LCD_WIDTH; x++) {
				sum += (pixels[x] & 0x03);
			}
			printf("DEBUG GPU: frame=%u LY=%u SCX=%u SCY=%u LCDC=0x%02X bg_map=0x%04X bg_y=0x%02X idx@159=0x%02X\n",
				gb->frame_debug,
				gb->hram_io[IO_LY],
				gb->hram_io[IO_SCX],
				gb->hram_io[IO_SCY],
				gb->hram_io[IO_LCDC],
				bg_map,
				bg_y,
				gb->vram[bg_map + ((159 + gb->hram_io[IO_SCX]) >> 3)]);
		}

		if (gb->hram_io[IO_LY] == 144) {
			printf("DEBUG: LY=144, setting VBLANK interrupt. IF before=%02X\n", 
				gb->hram_io[IO_IF]);
			gb->hram_io[IO_IF] |= 0x01;  // Set VBLANK interrupt
			printf("DEBUG: IF after=%02X\n", gb->hram_io[IO_IF]);
		}

		/* The displays (what the player sees) X coordinate, drawn right to left. */
		disp_x = LCD_WIDTH - 1;

		/* The X coordinate to begin drawing the background at. */
		bg_x = disp_x + gb->hram_io[IO_SCX];


		/* Get tile index for current background tile. */
		idx = gb->vram[bg_map + (bg_x >> 3)];

		// DEBUG: Check if we are fetching any non-zero tiles
		if (gb->hram_io[IO_LY] == 80 && idx != 0) {
			printf("LY=80 x=%u map=%04X idx=%02X\n", disp_x, bg_map, idx);
		}


		/* Y coordinate of tile pixel to draw. */
		py = (bg_y & 0x07);
		/* X coordinate of tile pixel to draw. */
		px = 7 - (bg_x & 0x07);

		// DEBUG: Check VRAM tile index and coordinates
		if (gb->hram_io[IO_LY] == 80 && idx == 0x39) {
			printf("DEBUG: py=%u px=%u bg_y=0x%02X bg_x=0x%02X\n", py, px, bg_y, bg_x);
		}

		/* Select addressing mode. */
		if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT){
			tile = VRAM_TILES_1 + idx * 0x10;
        } else {
			tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
        }
		tile += 2 * py;

		/* fetch first tile */
		t1 = gb->vram[tile] >> px;
		t2 = gb->vram[tile + 1] >> px;

		// DEBUG: Check VRAM tile data
		if (gb->hram_io[IO_LY] == 80 && (t1 != 0 || t2 != 0)) {
			printf("DEBUG: LY=80 tile data non-zero: t1=%02X t2=%02X\n", t1, t2);
			printf("  raw: vram[0x%04X]=0x%02X vram[0x%04X]=0x%02X\n",
           		tile, gb->vram[tile], tile+1, gb->vram[tile+1]);
    		printf("  shifted: t1=0x%02X t2=0x%02X\n", t1, t2);
		}

		for(; disp_x != 0xFF; disp_x--){
			uint8_t c;

			if(px == 8){
				/* fetch next tile */
				px = 0;
				bg_x = disp_x + gb->hram_io[IO_SCX];
				idx = gb->vram[bg_map + (bg_x >> 3)];

				if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT){
					tile = VRAM_TILES_1 + idx * 0x10;
                } else {
					tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
                }
				tile += 2 * py;
				t1 = gb->vram[tile];
				t2 = gb->vram[tile + 1];
			}

			/* copy background */
			c = (t1 & 0x1) | ((t2 & 0x1) << 1);
			pixels[disp_x] = gb->display.bg_palette[c];

			// if (gb->hram_io[IO_LY] == 80 && disp_x > 140 && disp_x <= 159) {
			// 	printf("DEBUG: LY=80 x=%3u c=%u bgpix=%u\n",
			// 		disp_x, c, pixels[disp_x]);
			// }

			t1 >>= 1;
			t2 >>= 1;
			px++;
		}
	}

	/* draw window */
	if(gb->hram_io[IO_LCDC] & LCDC_WINDOW_ENABLE && gb->hram_io[IO_LY] >= gb->display.WY && gb->hram_io[IO_WX] <= 166){
		uint16_t win_line, tile;
		uint8_t disp_x, win_x, py, px, idx, t1, t2, end;

		/* Calculate Window Map Address. */
		win_line = (gb->hram_io[IO_LCDC] & LCDC_WINDOW_MAP) ? VRAM_BMAP_2 : VRAM_BMAP_1;
		win_line += (gb->display.window_clear >> 3) * 0x20;

		disp_x = LCD_WIDTH - 1;
		win_x = disp_x - gb->hram_io[IO_WX] + 7;

		// look up tile
		py = gb->display.window_clear & 0x07;
		px = 7 - (win_x & 0x07);
		idx = gb->vram[win_line + (win_x >> 3)];

		if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT){
			tile = VRAM_TILES_1 + idx * 0x10;
        } else {
			tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
        }

		tile += 2 * py;

		// fetch first tile
		t1 = gb->vram[tile] >> px;
		t2 = gb->vram[tile + 1] >> px;

		// loop & copy window
		end = (gb->hram_io[IO_WX] < 7 ? 0 : gb->hram_io[IO_WX] - 7) - 1;

		for(; disp_x != end; disp_x--){
			uint8_t c;

			if(px == 8){
				// fetch next tile
				px = 0;
				win_x = disp_x - gb->hram_io[IO_WX] + 7;
				idx = gb->vram[win_line + (win_x >> 3)];

				if(gb->hram_io[IO_LCDC] & LCDC_TILE_SELECT){
					tile = VRAM_TILES_1 + idx * 0x10;
                } else {
					tile = VRAM_TILES_2 + ((idx + 0x80) % 0x100) * 0x10;
                }
				tile += 2 * py;
				t1 = gb->vram[tile];
				t2 = gb->vram[tile + 1];
			}

			// copy window
			c = (t1 & 0x1) | ((t2 & 0x1) << 1);
			pixels[disp_x] = gb->display.bg_palette[c];

			t1 = t1 >> 1;
			t2 = t2 >> 1;
			px++;
		}

		gb->display.window_clear++; // advance window line
	}

	// draw sprites
	if(gb->hram_io[IO_LCDC] & LCDC_OBJ_ENABLE){
		uint8_t sprite_number;

		for(sprite_number = NUM_SPRITES - 1; sprite_number != 0xFF; sprite_number--){
			uint8_t s = sprite_number;

			uint8_t py, t1, t2, dir, start, end, shift, disp_x;
			/* Sprite Y position. */
			uint8_t OY = gb->oam[4 * s + 0];
			/* Sprite X position. */
			uint8_t OX = gb->oam[4 * s + 1];
			/* Sprite Tile/Pattern Number. */
			uint8_t OT = gb->oam[4 * s + 2]
				     & (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 0xFE : 0xFF);
			/* Additional attributes. */
			uint8_t OF = gb->oam[4 * s + 3];

			/* If sprite isn't on this line, continue. */
			if(gb->hram_io[IO_LY] + (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 0 : 8) >= OY || gb->hram_io[IO_LY] + 16 < OY){
				continue;
            }

			/* Continue if sprite not visible. */
			if(OX == 0 || OX >= 168) continue;

			// y flip
			py = gb->hram_io[IO_LY] - OY + 16;

			if(OF & OBJ_FLIP_Y) py = (gb->hram_io[IO_LCDC] & LCDC_OBJ_SIZE ? 15 : 7) - py;

			// fetch the tile
			t1 = gb->vram[VRAM_TILES_1 + OT * 0x10 + 2 * py];
			t2 = gb->vram[VRAM_TILES_1 + OT * 0x10 + 2 * py + 1];

			// handle x flip
			// handle x flip and draw sprite pixels
			if (OF & OBJ_FLIP_X) {
				/* Sprite is flipped horizontally: draw left -> right. */
				dir   = 1;
				start = (OX < 8 ? 0 : OX - 8);
				end   = MIN(OX, LCD_WIDTH);
				shift = 8 - OX + start;

				/* Align tile bits with the first on-screen pixel. */
				t1 >>= shift;
				t2 >>= shift;

				if (OF & OBJ_PRIORITY) {
					/* Behind background: only draw over BG colour 0. */
					for (disp_x = start; disp_x != end; disp_x += dir) {
						uint8_t c = (t1 & 0x1) | ((t2 & 0x1) << 1);

						if (c && ((pixels[disp_x] & 0x3) == gb->display.bg_palette[0])) {
							pixels[disp_x] = (OF & OBJ_PALETTE)
											? gb->display.sp_palette[c + 4]
											: gb->display.sp_palette[c];
						}

						t1 >>= 1;
						t2 >>= 1;
					}
				} else {
					/* In front of background: any non-zero sprite pixel wins. */
					for (disp_x = start; disp_x != end; disp_x += dir) {
						uint8_t c = (t1 & 0x1) | ((t2 & 0x1) << 1);

						if (c) {
							pixels[disp_x] = (OF & OBJ_PALETTE)
											? gb->display.sp_palette[c + 4]
											: gb->display.sp_palette[c];
						}

						t1 >>= 1;
						t2 >>= 1;
					}
				}
			} else {
				/* Not flipped: draw right -> left. */
				dir   = (uint8_t)-1;
				start = MIN(OX, LCD_WIDTH) - 1;
				end   = (OX < 8 ? 0 : OX - 8) - 1;
				shift = OX - (start + 1);

				/* Align tile bits with the first on-screen pixel. */
				t1 >>= shift;
				t2 >>= shift;

				if (OF & OBJ_PRIORITY) {
					/* Behind background: only draw over BG colour 0. */
					for (disp_x = start; disp_x != end; disp_x += dir) {
						uint8_t c = (t1 & 0x1) | ((t2 & 0x1) << 1);

						if (c && ((pixels[disp_x] & 0x3) == gb->display.bg_palette[0])) {
							pixels[disp_x] = (OF & OBJ_PALETTE)
											? gb->display.sp_palette[c + 4]
											: gb->display.sp_palette[c];
						}

						t1 >>= 1;
						t2 >>= 1;
					}
				} else {
					/* In front of background: any non-zero sprite pixel wins. */
					for (disp_x = start; disp_x != end; disp_x += dir) {
						uint8_t c = (t1 & 0x1) | ((t2 & 0x1) << 1);

						if (c) {
							pixels[disp_x] = (OF & OBJ_PALETTE)
											? gb->display.sp_palette[c + 4]
											: gb->display.sp_palette[c];
						}

						t1 >>= 1;
						t2 >>= 1;
					}
				}
			}
		}
	}

	// DEBUG: inspect line contents for a few key lines
    // if (gb->hram_io[IO_LY] == 0 || gb->hram_io[IO_LY] == 80 || gb->hram_io[IO_LY] == 143) {
    //     uint32_t sum = 0;
    //     for (int x = 0; x < LCD_WIDTH; x++) {
    //         sum += (pixels[x] & 0x03);
    //     }
    //     printf("DEBUG: GPU LINE %u: first=%u last=%u sum=%u\n",
    //            gb->hram_io[IO_LY],
    //            pixels[0] & 0x03,
    //            pixels[LCD_WIDTH - 1] & 0x03,
    //            sum);
    // }

	gb->display.lcd_draw_line(gb, pixels, gb->hram_io[IO_LY]);

	if (gb->hram_io[IO_LY] == 0 && gb->frame_debug < 5) {
		printf("DEBUG GPU CALLED: frame=%u LY=%u first=%u last=%u\n",
			gb->frame_debug,
			gb->hram_io[IO_LY],
			pixels[0] & 0x03,
			pixels[LCD_WIDTH - 1] & 0x03);
	}
}