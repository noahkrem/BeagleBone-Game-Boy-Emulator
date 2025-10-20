#include "gb_types.h"

#ifndef ROM_H
#define ROM_H

#define TETRIS_ROM_PATH "rom/tetris.gb"

/**
 * Read byte from ROM
 * @param gb    Emulator context
 * @param bank  ROM bank number
 * @param addr  16-bit address to read from
 * @return      Byte at address
 */
uint8_t gb_rom_read(struct gb_s* gb, uint8_t bank, uint16_t addr);

/** 
 *  Bootloader function to initialize and return a pointer to the main emulator context.
 *  @param rom_path Path to the ROM file to load.
 *  @return Pointer to initialized gb_s struct, or NULL on failure.
 */
gb_s* bootloader(char* rom_path);





#endif // ROM_H