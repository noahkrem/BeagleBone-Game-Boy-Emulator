
/*
 * Bootloader implementation for Game Boy Emulator
 * Author: Dan Renardson
 * Date: 2025-10-20
 *
 * This file contains the bootloader code responsible for initializing
 * the emulator and loading the game ROM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rom.h"
#include "gb_types.h"


gb_read_byte_t gb_rom_read(struct gb_s* gb, uint8_t bank, uint16_t addr) {

    // Fail condition return values
    gb_read_byte_t result = {0, false};

    if(!gb) {
        perror("gb_rom_read: NULL gb_s pointer");
        return result;
    }
    if(!gb->rom) {
        perror("gb_rom_read: NULL ROM pointer in gb_s struct");
        return result;
    }
    if(addr >= ROM_BANK_SIZE) {
        perror("gb_rom_read: Address out of range, addr = ");
        perror((char[]){(char)(addr >> 8), (char)(addr & 0xFF), '\0'});
        return result;
    }
    if(bank >= MAX_ROM_BANKS) {
        perror("gb_rom_read: Bank number out of range, bank = ");
        perror((char[]){(char)bank, '\0'});
        return result;
    }

    // Calculate the actual address in the ROM array
    size_t rom_addr = (size_t)(bank * ROM_BANK_SIZE + addr);

    // Return the byte at the address of the entire ROM block
    return gb->rom[rom_addr];
}

struct gb_s* bootloader(char* rom_path) {

    // Open the ROM file
    FILE* rom_file = fopen(rom_path, "rb");
    if (!rom_file) {
        perror("bootloader: Failed to open ROM file at:");
        perror(rom_path);
        return NULL;
    }

    // Seek to offset 0x0148 in the file (where the size of the ROM is stored)
    if (fseek(rom_file, 0x0148, SEEK_SET) != 0) {
        perror("bootloader: fseek at 0x0148 failed");
        fclose(rom_file);
        return NULL;
    }

    // Read the ROM size byte
    uint8_t rom_size;
    if (fread(&rom_size, sizeof(uint8_t), 1, rom_file) != 1) {
        perror("bootloader: fread at 0x0148 failed");
        fclose(rom_file);
        return NULL;
    }

    /*
     * Extract the ROM size from the byte at 0x0148.
     * Address 0x0148 -> ROM size:
     * 0 - 256Kbit = 32KByte = 2 banks
     * 1 - 512Kbit = 64KByte = 4 banks
     * 2 - 1Mbit = 128KByte = 8 banks
     * 3 - 2Mbit = 256KByte = 16 banks
     * 4 - 4Mbit = 512KByte = 32 banks
     * 5 - 8Mbit = 1MByte = 64 banks
     * 6 - 16Mbit = 2MByte = 128 banks
     * 0x52 - 9Mbit = 1.1MByte = 72 banks
     * 0x53 - 10Mbit = 1.2MByte = 80 banks
     * 0x54 - 12Mbit = 1.5MByte = 96 banks
     */
    switch (rom_size) {
        case 0x00: rom_size = 2*ROM_BANK_SIZE; break;
        case 0x01: rom_size = 4*ROM_BANK_SIZE; break;
        case 0x02: rom_size = 8*ROM_BANK_SIZE; break;
        case 0x03: rom_size = 16*ROM_BANK_SIZE; break;
        case 0x04: rom_size = 32*ROM_BANK_SIZE; break;
        case 0x05: rom_size = 64*ROM_BANK_SIZE; break;
        case 0x06: rom_size = 128*ROM_BANK_SIZE; break;
        case 0x52: rom_size = 72*ROM_BANK_SIZE; break;
        case 0x53: rom_size = 80*ROM_BANK_SIZE; break;
        case 0x54: rom_size = 96*ROM_BANK_SIZE; break;
        default:
            perror("bootloader: Unsupported ROM size code:");
            perror((char[]){rom_size, '\0'});
            fclose(rom_file);
            return NULL;
    }

    // Allocate memory for the emulator context struct itself
    struct gb_s* gb = (struct gb_s*)malloc(sizeof(struct gb_s));
    if (!gb) {
        perror("bootloader: Failed to allocate memory for emulator context");
        return NULL;
    }

    // Allocate memory for the ROM data read from file
    gb->rom = (uint8_t*)malloc(rom_size);
    if (!gb->rom) {
        perror("bootloader: Failed to allocate memory for ROM");
        fclose(rom_file);
        free(gb);
        return NULL;
    }

    // Seek back to the beginning of the ROM file
    if (fseek(rom_file, (size_t)0, SEEK_SET) != 0) {
        perror("bootloader: fseek to beginning failed");
        fclose(rom_file);
        free(gb->rom);
        free(gb);
        return NULL;
    }

    // Read the ROM data into the emulator context, each element to read is one byte
    if (fread(gb->rom, (size_t)1, rom_size, rom_file) != rom_size) {
        perror("bootloader: Load ROM data into gb_s struct failed");
        fclose(rom_file);
        free(gb->rom);
        free(gb);
        return NULL;
    }

    // Now that the ROM is loaded into the gb_s struct we can use the regular member functions to read from it
    // We no longer need the file open
    fclose(rom_file);


    // Assign the fucntion pointers in the gb_s struct to the defined functions
    
    return gb;
}
