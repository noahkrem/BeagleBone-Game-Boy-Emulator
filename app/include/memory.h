/**
 * memory.h - Memory Management Unit (MMU)
 * Author: Noah Kremler
 * Date: 2025-11-03
 * 
 * Handles: all memory access and banking for the Game Boy.
 * Implements the complete memory map and MBC1 banking.
 * 
 * Memory Map:
 * 0x0000-0x3FFF : ROM Bank 0 (fixed)
 * 0x4000-0x7FFF : ROM Bank N (switchable via MBC)
 * 0x8000-0x9FFF : Video RAM (VRAM)
 * 0xA000-0xBFFF : External RAM (Cart RAM)
 * 0xC000-0xDFFF : Work RAM (WRAM)
 * 0xE000-0xFDFF : Echo RAM (mirror of WRAM)
 * 0xFE00-0xFE9F : Object Attribute Memory (OAM)
 * 0xFEA0-0xFEFF : Not usable
 * 0xFF00-0xFF7F : I/O Registers
 * 0xFF80-0xFFFE : High RAM (HRAM)
 * 0xFFFF        : Interrupt Enable Register
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "gb_types.h"


// ----------------------------------
// Core Memory Access Functions
// ----------------------------------

/**
 * Read a byte from any memory address
 * 
 * This is the main memory read function used by the CPU and other components.
 * It handles all memory mapping, banking, and special cases.
 * 
 * @param gb    Emulator context
 * @param addr  16-bit address to read from
 * @return      Byte value at address
 */
uint8_t mmu_read(struct gb_s *gb, uint16_t addr);

/**
 * Write a byte to any memory address
 * 
 * This is the main memory write function used by the CPU and other components.
 * It handles all memory mapping, banking, ROM writes (for MBC control), 
 * and special registers.
 * 
 * @param gb    Emulator context
 * @param addr  16-bit address to write to
 * @param val   Byte value to write
 */
void mmu_write(struct gb_s *gb, uint16_t addr, uint8_t val);


// ----------------------------------
// Initialization
// ----------------------------------

/**
 * Initialize memory system
 * 
 * Sets up initial memory state, detects cartridge type, and configures
 * banking. Should be called after gb_init() but before CPU execution.
 * 
 * @param gb    Emulator context
 */
void mmu_init(struct gb_s *gb);

/**
 * Reset memory to initial state
 * 
 * Clears RAM, resets banking registers, and sets I/O registers to
 * power-on values.
 * 
 * @param gb    Emulator context
 */
void mmu_reset(struct gb_s *gb);

// ----------------------------------
// Cartridge Functions
// ----------------------------------

/**
 * Get the size of cartridge RAM (save data size)
 * 
 * Reads the ROM header to determine how much RAM the cartridge has.
 * This is needed to allocate the correct save file size.
 * 
 * @param gb        Emulator context
 * @param ram_size  Pointer to store the RAM size in bytes
 * @return          0 on success, -1 if RAM size is invalid
 */
int mmu_get_save_size(struct gb_s *gb, uint32_t *ram_size);


// ----------------------------------
// DMA (Direct Memory Access) Functions
// ----------------------------------

/**
 * Perform DMA transfer to OAM
 * 
 * Copies 160 bytes from source address to OAM (sprite attributes).
 * This is triggered by writing to the DMA register (0xFF46).
 * 
 * @param gb            Emulator context
 * @param source_high   High byte of source address (source is source_high << 8)
 */
void mmu_dma_transfer(struct gb_s *gb, uint8_t source_high);


// ----------------------------------
// Memory Region Constants
// ----------------------------------

/* Memory region boundaries */
#define MEM_ROM_BANK_0_START    0x0000
#define MEM_ROM_BANK_0_END      0x3FFF
#define MEM_ROM_BANK_N_START    0x4000
#define MEM_ROM_BANK_N_END      0x7FFF
#define MEM_VRAM_START          0x8000
#define MEM_VRAM_END            0x9FFF
#define MEM_CART_RAM_START      0xA000
#define MEM_CART_RAM_END        0xBFFF
#define MEM_WRAM_START          0xC000
#define MEM_WRAM_END            0xDFFF
#define MEM_ECHO_START          0xE000
#define MEM_ECHO_END            0xFDFF
#define MEM_OAM_START           0xFE00
#define MEM_OAM_END             0xFE9F
#define MEM_UNUSED_START        0xFEA0
#define MEM_UNUSED_END          0xFEFF
#define MEM_IO_START            0xFF00
#define MEM_IO_END              0xFF7F
#define MEM_HRAM_START          0xFF80
#define MEM_HRAM_END            0xFFFE
#define MEM_IE_REG              0xFFFF

/* ROM header locations */
#define ROM_HEADER_TITLE_START      0x0134
#define ROM_HEADER_TITLE_END        0x0143
#define ROM_HEADER_CART_TYPE        0x0147
#define ROM_HEADER_ROM_SIZE         0x0148
#define ROM_HEADER_RAM_SIZE         0x0149
#define ROM_HEADER_CHECKSUM         0x014D

/* MBC types */
#define MBC_TYPE_NONE               0
#define MBC_TYPE_MBC1               1
#define MBC_TYPE_MBC2               2
#define MBC_TYPE_MBC3               3
#define MBC_TYPE_MBC5               5


// ----------------------------------
// Helper Macros
// ----------------------------------

/* Check if address is in a specific memory region */
#define MMU_IS_ROM_BANK_0(addr)     ((addr) <= MEM_ROM_BANK_0_END)
#define MMU_IS_ROM_BANK_N(addr)     ((addr) >= MEM_ROM_BANK_N_START && (addr) <= MEM_ROM_BANK_N_END)
#define MMU_IS_VRAM(addr)           ((addr) >= MEM_VRAM_START && (addr) <= MEM_VRAM_END)
#define MMU_IS_CART_RAM(addr)       ((addr) >= MEM_CART_RAM_START && (addr) <= MEM_CART_RAM_END)
#define MMU_IS_WRAM(addr)           ((addr) >= MEM_WRAM_START && (addr) <= MEM_WRAM_END)
#define MMU_IS_OAM(addr)            ((addr) >= MEM_OAM_START && (addr) <= MEM_OAM_END)
#define MMU_IS_IO(addr)             ((addr) >= MEM_IO_START && (addr) <= MEM_IO_END)
#define MMU_IS_HRAM(addr)           ((addr) >= MEM_HRAM_START && (addr) <= MEM_HRAM_END)

/* Convert address to offset within memory region */
#define MMU_VRAM_OFFSET(addr)       ((addr) - MEM_VRAM_START)
#define MMU_CART_RAM_OFFSET(addr)   ((addr) - MEM_CART_RAM_START)
#define MMU_WRAM_OFFSET(addr)       ((addr) - MEM_WRAM_START)
#define MMU_OAM_OFFSET(addr)        ((addr) - MEM_OAM_START)
#define MMU_IO_OFFSET(addr)         ((addr) - MEM_IO_START)
#define MMU_HRAM_OFFSET(addr)       ((addr) - MEM_HRAM_START)

#endif  // MEMORY_H