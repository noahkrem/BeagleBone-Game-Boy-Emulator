#include "gb_types.h"

#ifndef ROM_H
#define ROM_H


#define DEBUG_ROM 1 // Debug macro that prints lots of info about ROM
#define TETRIS_ROM_PATH "rom/tetris.gb" // Path to Tetris ROM file

#define NUM_CART_ROM_BANKS_2KB 255 // value to assign to num_cart_ram_banks for 2KB RAM size

#define NINTENDO_START_ADDR 0x0104 // start and end addresses for the scrolling Nintendo 
#define NINTENDO_END_ADDR   0x0133

#define TITLE_START_ADDR 0x0134 // start and end addresses for the game title in the ROM header
#define TITLE_END_ADDR   0x0143

/**
 * Read byte from ROM. It is the caller's responsibility to have the correct block selected.
 * @param gb    Emulator context
 * @param addr  16-bit address to read from
 * @return      Byte at address
 */
gb_read_byte_t gb_rom_read(struct gb_s* gb, uint16_t addr);

/**
 * Select ROM bank.
 * @param gb    Emulator context
 * @param bank  ROM bank number to select
 * @return      TRUE on success, FALSE on failure
 */
bool gb_rom_select_bank(struct gb_s* gb, uint8_t bank);

/** 
 *  Bootloader function to initialize and return a pointer to the main emulator context.
 *  @param rom_path Path to the ROM file to load.
 *  @return Pointer to initialized gb_s struct, or NULL on failure.
 */
struct gb_s* bootloader(char* rom_path);





#endif // ROM_H