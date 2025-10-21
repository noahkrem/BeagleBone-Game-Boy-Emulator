#include "gb_types.h"

#ifndef ROM_H
#define ROM_H

// Debug macro that prints lots of info about ROM
#define DEBUG_ROM 1

// Path to Tetris ROM file
#define TETRIS_ROM_PATH "rom/tetris.gb"

/**
 * Read byte from ROM. It is the caller's responsibility to have the correct block selected.
 * @param gb    Emulator context
 * @param addr  16-bit address to read from
 * @return      Byte at address
 */
uint8_t gb_rom_read(struct gb_s* gb, uint16_t addr);

/**
 * Select ROM bank.
 * @param gb    Emulator context
 * @param bank  ROM bank number to select
 * @return      0 on success, -1 on failure
 */
int8_t gb_rom_select_bank(struct gb_s* gb, uint8_t bank);

/** 
 *  Bootloader function to initialize and return a pointer to the main emulator context.
 *  @param rom_path Path to the ROM file to load.
 *  @return Pointer to initialized gb_s struct, or NULL on failure.
 */
gb_s* bootloader(char* rom_path);





#endif // ROM_H