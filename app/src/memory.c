/**
 * memory.c - Memory Management Unit Implementation
 * 
 * Implements complete Game Boy memory map with MBC1 banking support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "memory.h"
#include "gb_types.h"

/* External framebuffer from main.c */
extern uint16_t fb[144][160];


// ----------------------------------
// Memory Read Function
// ----------------------------------

uint8_t mmu_read(struct gb_s *gb, uint16_t addr) {
    /* ROM Bank 0 (0x0000 - 0x3FFF) - Always mapped */
    if (addr < 0x4000) {
        return gb->gb_rom_read(gb, addr);
    }
    
    // ROM Bank N (0x4000 - 0x7FFF) - Switchable 
    // Bugfix: This code previously used the wrong calculation for the ROM address for ROM bank 1.
    //   The fix was to update the formula to "ROM Address = CPU Address + (Selected Bank - 1) * Bank Size".
    //   This formula includes the correct base offset for ROM bank 1, enabling us to read the correct data 
    //   relative to the start of the ROM file.
    else if (addr < 0x8000) {
        /* Calculate offset based on selected bank */
        if(gb->mbc == 1 && gb->cart_mode_select){
			return gb->gb_rom_read(gb, addr + ((gb->selected_rom_bank & 0x1F) - 1) * ROM_BANK_SIZE);
        } else {
			return gb->gb_rom_read(gb, addr + (gb->selected_rom_bank - 1) * ROM_BANK_SIZE);
        }
    }
    
    /* Video RAM (0x8000 - 0x9FFF) */
    else if (addr < 0xA000) {
        return gb->vram[addr - 0x8000];
    }
    
    /* External RAM (0xA000 - 0xBFFF) - Cartridge RAM */
    else if (addr < 0xC000) {
        /* Check if cart RAM is enabled */
        if (!gb->cart_ram || !gb->enable_cart_ram) {
            return 0xFF;  /* Return 0xFF when RAM is disabled */
        }
        
        /* Calculate RAM address with banking */
        uint32_t ram_offset = addr - 0xA000;
        
        /* Handle different MBC modes */
        if (gb->mbc == 1) {
            /* MBC1: In mode 1, RAM banking is enabled */
            if (gb->cart_mode_select && gb->cart_ram_bank < gb->num_ram_banks) {
                ram_offset += gb->cart_ram_bank * CRAM_BANK_SIZE;
            }
        }
        
        return gb->gb_cart_ram_read(gb, ram_offset);
    }
    
    /* Work RAM (0xC000 - 0xDFFF) */
    else if (addr < 0xE000) {
        return gb->wram[addr - 0xC000];
    }
    
    /* Echo RAM (0xE000 - 0xFDFF) - Mirror of WRAM */
    else if (addr < 0xFE00) {
        return gb->wram[addr - 0xE000];
    }
    
    /* Object Attribute Memory (0xFE00 - 0xFE9F) - Sprite data */
    else if (addr < 0xFEA0) {
        return gb->oam[addr - 0xFE00];
    }
    
    /* Unusable memory (0xFEA0 - 0xFEFF) */
    else if (addr < 0xFF00) {
        return 0xFF;
    }
    
    // I/O Registers and High RAM (0xFF00 - 0xFFFF)
    // Effectively the "no row selected" case for JOYP
    // The JOYP register is a 2×4 matrix:
    //   Bits 4–5 select which half (d‑pad vs buttons) the game wants.
    //   Bits 0–3 return the state of that half (0 = pressed, 1 = released).
    //   If neither bit 4 nor bit 5 is cleared (i.e., both are 1), the game hasn’t selected anything; 
    //     conceptually, “no keys are being scanned” and you typically return all 1s (no key pressed).
    else {
        /* Special handling for joypad register */
        if (addr == 0xFF00) {
            uint8_t joyp = gb->hram_io[IO_JOYP];
            uint8_t result = joyp | 0x0F;  // Start with low nibble = 1111 (all released)
            
            /* If direction keys selected (bit 4 = 0) */
            if ((joyp & 0x10) == 0) {
                // AND with direction bits (right, left, up, down)
                result &= (gb->direct.joypad >> 4) | 0xF0;
            }
            /* If button keys selected (bit 5 = 0) */
            else if ((joyp & 0x20) == 0) {
                // AND with button bits (a, b, select, start)
                result &= gb->direct.joypad | 0xF0;
            }
            
            return result;
        }
        
        /* All other I/O and HRAM */
        return gb->hram_io[addr - 0xFF00];
    }
}


// ----------------------------------
// Memory Write Function
// ----------------------------------

void mmu_write(struct gb_s *gb, uint16_t addr, uint8_t val) {
    /* ROM area (0x0000 - 0x7FFF) - MBC banking control */
    if (addr < 0x8000) {
        /* Only handle MBC1 for MVP */
        if (gb->mbc != 1) {
            return;  /* Ignore writes if no MBC or unsupported MBC */
        }
        
        /* MBC1 Banking */
        if (addr < 0x2000) {
            /* RAM Enable (0x0000 - 0x1FFF) */
            gb->enable_cart_ram = ((val & 0x0F) == 0x0A);
        }
        else if (addr < 0x4000) {
            /* ROM Bank Number (0x2000 - 0x3FFF) */
            gb->selected_rom_bank = (val & 0x1F) | (gb->selected_rom_bank & 0x60);
            
            /* Bank 0 is not directly accessible in switchable area */
            if ((gb->selected_rom_bank & 0x1F) == 0) {
                gb->selected_rom_bank++;
            }
            
            /* Mask to valid ROM banks */
            gb->selected_rom_bank &= gb->num_rom_banks_mask;
        }
        else if (addr < 0x6000) {
            /* RAM Bank Number / Upper bits of ROM Bank (0x4000 - 0x5FFF) */
            gb->cart_ram_bank = val & 0x03;
            gb->selected_rom_bank = ((val & 0x03) << 5) | (gb->selected_rom_bank & 0x1F);
            gb->selected_rom_bank &= gb->num_rom_banks_mask;
        }
        else {
            /* Banking Mode Select (0x6000 - 0x7FFF) */
            gb->cart_mode_select = val & 0x01;
        }
        return;
    }
    
    /* Video RAM (0x8000 - 0x9FFF) */
    else if (addr < 0xA000) {
        gb->vram[addr - 0x8000] = val;
    }
    
    /* External RAM (0xA000 - 0xBFFF) */
    else if (addr < 0xC000) {
        /* Check if cart RAM is enabled */
        if (!gb->cart_ram || !gb->enable_cart_ram) {
            return;
        }
        
        /* Calculate RAM address with banking */
        uint32_t ram_offset = addr - 0xA000;
        
        /* Handle different MBC modes */
        if (gb->mbc == 1) {
            /* MBC1: In mode 1, RAM banking is enabled */
            if (gb->cart_mode_select && gb->cart_ram_bank < gb->num_ram_banks) {
                ram_offset += gb->cart_ram_bank * CRAM_BANK_SIZE;
            }
        }
        
        gb->gb_cart_ram_write(gb, ram_offset, val);
    }
    
    /* Work RAM (0xC000 - 0xDFFF) */
    else if (addr < 0xE000) {
        gb->wram[addr - 0xC000] = val;
    }
    
    /* Echo RAM (0xE000 - 0xFDFF) - Mirror of WRAM */
    else if (addr < 0xFE00) {
        gb->wram[addr - 0xE000] = val;
    }
    
    /* Object Attribute Memory (0xFE00 - 0xFE9F) */
    else if (addr < 0xFEA0) {
        gb->oam[addr - 0xFE00] = val;
    }
    
    /* Unusable memory (0xFEA0 - 0xFEFF) - Ignore writes */
    else if (addr < 0xFF00) {
        return;
    }
    
    /* I/O Registers and High RAM (0xFF00 - 0xFFFF) */
    else {
        /* Handle special I/O registers */
        uint8_t io_offset = addr - 0xFF00;
        
        switch (io_offset) {
            case IO_JOYP: /* Joypad (0xFF00) */
                /* Only bits 4 and 5 are writable */
                gb->hram_io[IO_JOYP] = (val & 0x30) | 0xC0;
                break;
            
            case IO_DIV: /* Divider Register (0xFF04) */
                /* Writing any value resets DIV to 0 */
                gb->hram_io[IO_DIV] = 0;
                gb->counter.div_count = 0;
                break;
            
            case IO_DMA: /* DMA Transfer (0xFF46) */
                gb->hram_io[IO_DMA] = val;
                mmu_dma_transfer(gb, val);
                break;
            
            case IO_BGP: /* Background Palette (0xFF47) */
                gb->hram_io[IO_BGP] = val;
                /* Update palette data */
                gb->display.bg_palette[0] = (val >> 0) & 0x03;
                gb->display.bg_palette[1] = (val >> 2) & 0x03;
                gb->display.bg_palette[2] = (val >> 4) & 0x03;
                gb->display.bg_palette[3] = (val >> 6) & 0x03;
                break;
            
            case IO_OBP0: /* Object Palette 0 (0xFF48) */
                gb->hram_io[IO_OBP0] = val;
                /* Update palette data */
                gb->display.sp_palette[0] = (val >> 0) & 0x03;
                gb->display.sp_palette[1] = (val >> 2) & 0x03;
                gb->display.sp_palette[2] = (val >> 4) & 0x03;
                gb->display.sp_palette[3] = (val >> 6) & 0x03;
                break;
            
            case IO_OBP1: /* Object Palette 1 (0xFF49) */
                gb->hram_io[IO_OBP1] = val;
                /* Update palette data */
                gb->display.sp_palette[4] = (val >> 0) & 0x03;
                gb->display.sp_palette[5] = (val >> 2) & 0x03;
                gb->display.sp_palette[6] = (val >> 4) & 0x03;
                gb->display.sp_palette[7] = (val >> 6) & 0x03;
                break;
            
            case IO_LCDC: /* LCD Control (0xFF40) */
            {
                uint8_t old = gb->hram_io[IO_LCDC];
                uint8_t lcd_was_on = old & LCDC_ENABLE;
                gb->hram_io[IO_LCDC] = val;
                uint8_t lcd_is_now_on = val & LCDC_ENABLE;
                
                if (!lcd_was_on && lcd_is_now_on) {
                    gb->lcd_blank = true;
                    gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | LCD_MODE_OAM_SCAN;
                    gb->hram_io[IO_LY] = 0;
                    gb->counter.lcd_count = 0;
                }
                else if (lcd_was_on && !lcd_is_now_on) {
                    gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | LCD_MODE_HBLANK;
                    gb->hram_io[IO_LY] = 0;
                    gb->counter.lcd_count = 0;
                }
                break;
            }
            
            case IO_STAT: /* LCD Status (0xFF41) */
                /* Only bits 3-6 are writable, bits 0-2 are read-only */
                gb->hram_io[IO_STAT] = (val & 0x78) | (gb->hram_io[IO_STAT] & 0x07) | 0x80;
                break;
            
            case IO_LY: /* LY is read-only, ignore writes */
                break;
            
            case IO_IF: /* Interrupt Flags (0xFF0F) */
                /* Upper 3 bits always read as 1 */
                gb->hram_io[IO_IF] = val | 0xE0;
                break;
            
            case IO_IE: /* Interrupt Enable (0xFF) */
                gb->hram_io[IO_IE] = val;
                break;

            case IO_SCY: /* Scroll Y (0xFF42) */
                gb->hram_io[IO_SCY] = val;
                break;

            case IO_SCX: /* Scroll X (0xFF43) */
                gb->hram_io[IO_SCX] = val;
                break;

            /* Also ensure you have these while you're here */
            case IO_WX: /* Window X (0xFF4B) */
                gb->hram_io[IO_WX] = val;
                break;

            case IO_WY: /* Window Y (0xFF4A) */
                gb->display.WY = val;  /* Store wherever your PPU reads WY from */
                break;

            
            default:
                /* All other I/O registers and HRAM */
                gb->hram_io[io_offset] = val;
                break;
        }
    }
}


// ----------------------------------
// DMA Transfer
// ----------------------------------

void mmu_dma_transfer(struct gb_s *gb, uint8_t source_high) {
    uint16_t source = source_high << 8;
    
    /* Copy 160 bytes from source to OAM */
    for (uint16_t i = 0; i < OAM_SIZE; i++) {
        gb->oam[i] = mmu_read(gb, source + i);
    }
}

// ----------------------------------
// Initialization Functions
// ----------------------------------

void mmu_init(struct gb_s *gb) {
    /* Clear all memory */
    memset(gb->wram, 0, WRAM_SIZE);
    memset(gb->vram, 0, VRAM_SIZE);
    memset(gb->oam, 0, OAM_SIZE);
    memset(gb->hram_io, 0, HRAM_IO_SIZE);
    
    /* Initialize I/O registers to power-on state */
    gb->hram_io[IO_JOYP] = 0xCF;
    gb->hram_io[IO_DIV] = 0xAB;
    gb->hram_io[IO_IF] = 0xE1;
    gb->hram_io[IO_LCDC] = 0x91;
    gb->hram_io[IO_STAT] = 0x85;
    gb->hram_io[IO_BGP] = 0xFC;
    gb->hram_io[IO_OBP0] = 0xFF;
    gb->hram_io[IO_OBP1] = 0xFF;
    
    /* Update palette arrays */
    mmu_write(gb, 0xFF47, 0xFC);  /* BGP */
    mmu_write(gb, 0xFF48, 0xFF);  /* OBP0 */
    mmu_write(gb, 0xFF49, 0xFF);  /* OBP1 */
    
    /* Initialize banking */
    gb->selected_rom_bank = 1;
    gb->cart_ram_bank = 0;
    gb->enable_cart_ram = 0;
    gb->cart_mode_select = 0;
}

void mmu_reset(struct gb_s *gb) {
    /* Same as init for now */
    mmu_init(gb);
}


// ----------------------------------
// Cartridge Functions
// ----------------------------------

int mmu_get_save_size(struct gb_s *gb, uint32_t *ram_size) {
    /* RAM size lookup table based on ROM header value */
    static const uint32_t ram_sizes[] = {
        0x0000,     /* 0: No RAM */
        0x0800,     /* 1: 2KB (rarely used) */
        0x2000,     /* 2: 8KB */
        0x8000,     /* 3: 32KB (4 banks) */
        0x20000,    /* 4: 128KB (16 banks) */
        0x10000     /* 5: 64KB (8 banks) */
    };
    
    uint8_t ram_size_code = mmu_read(gb, ROM_HEADER_RAM_SIZE);
    
    /* Check for valid RAM size code */
    if (ram_size_code >= sizeof(ram_sizes) / sizeof(ram_sizes[0])) {
        return -1;  /* Invalid RAM size */
    }
    
    *ram_size = ram_sizes[ram_size_code];
    return 0;
}