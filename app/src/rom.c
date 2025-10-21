
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


gb_read_byte_t gb_rom_read(struct gb_s* gb, uint16_t addr) {

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

    // if an attempt is made to read from ROM that is at address >= 0x3FFF then that's an error automatically
    // [0x0000, 0x1FFF] => Block 0
    // [0x2000, 0x3FFF] => Selected Block
    if(addr >= (2*ROM_BANK_SIZE)-1) {
        perror("gb_rom_read: Address out of range, addr = ");
        perror((char[]){(char)(addr >> 8), (char)(addr & 0xFF), '\0'});
        return result;
    }

    // Calculate the actual address in the ROM array
    size_t rom_addr = (size_t)(gb->selected_rom_bank * ROM_BANK_SIZE + addr);

    // Return the byte at the address of the entire ROM block
    result.byte = gb->rom[rom_addr];
    result.valid = true;
    return result;
}



int8_t gb_rom_select_bank(struct gb_s* gb, uint8_t bank) {

    if(!gb) {
        perror("gb_rom_select_bank: NULL gb_s pointer");
        return -1;
    }

    if(!gb->mbc) {
        perror("gb_rom_select_bank: Cartridge has no MBC, cannot select bank = ");
        perror((char[]){(char)bank, '\0'});
        return -1;
    }

    // Bank 0 is always available in addresses [0x0000, 0x1FFF], so selecting it is not allowed
    if(bank == 0) {
        perror("gb_rom_select_bank: Cannot select bank 0, remapping to bank 1\n");
        bank = 1;   
    }

    // This checks whether the caller is attempting to select a block that is too high to exist on any cartrige
    // Later we check against the actual loaded ROM size
    if(bank >= gb->num_rom_banks) {
        perror("gb_rom_select_bank: Bank number out of range, bank = ");
        perror((char[]){(char)bank, '\0'});
        perror(" , max banks = ");
        perror((char[]){(char)(gb->num_rom_banks), '\0'});
        return -1;
    }

    gb->selected_rom_bank = bank;
    return 0;
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
    uint8_t rom_size_from_cart;
    if (fread(&rom_size_from_cart, sizeof(uint8_t), 1, rom_file) != 1) {
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
    uint8_t num_rom_banks = 0;
    switch (rom_size_from_cart) {
        case 0x00: num_rom_banks = 2; break;
        case 0x01: num_rom_banks = 4; break;
        case 0x02: num_rom_banks = 8; break;
        case 0x03: num_rom_banks = 16; break;
        case 0x04: num_rom_banks = 32; break;
        case 0x05: num_rom_banks = 64; break;
        case 0x06: num_rom_banks = 128; break;
        case 0x52: num_rom_banks = 72; break;
        case 0x53: num_rom_banks = 80; break;
        case 0x54: num_rom_banks = 96; break;
        default:
            perror("bootloader: Unsupported ROM size code:");
            perror((char[]){rom_size_from_cart, '\0'});
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
    uint64_t rom_size = (num_rom_banks * ROM_BANK_SIZE);
    gb->num_rom_banks = num_rom_banks;
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

    // Initialize the selected ROM bank to bank 0
    gb_rom_select_bank(gb, 0);

    // Extract the cartrige type from address 0x0147
    gb_read_byte_t cart_type_read = gb_rom_read(gb, 0x0147);
    if(!cart_type_read.valid) {
        perror("bootloader: Failed to read cartridge type from ROM");
        free(gb->rom);
        free(gb);
        return NULL;
    }
    switch (cart_type_read.byte) {
        case 0x00: // No MBC + no cartridge RAM
            gb->mbc = 0;
            gb->cart_ram_enable = 0;
            gb->num_cart_ram_banks = 0;
            break;
        case 0x01: // MBC1 + not cartridge RAM
            gb->mbc = 1;
            gb->cart_ram_enable = 0;
            break;
        case 0x02: // MBC1 + RAM
            gb->mbc = 1;
            gb->cart_ram_enable = 1;
            break;
        case 0x03: // MBC1 + RAM + Battery (same as 0x02 for our purposes)
            gb->mbc = 1;
            gb->cart_ram_enable = 1;
            break;
        case 0x08: // No MBC + RAM 
            gb->mbc = 0;
            gb->cart_ram_enable = 1;
            gb->num_cart_ram_banks = 1; // No block switching, must be 8KB of on-cartridge RAM
            break;
        case 0x09: // No MBC + RAM + Battery (same as 0x08 for our purposes)
            gb->mbc = 0;
            gb->cart_ram_enable = 1;
            gb->num_cart_ram_banks = 1; // Must be 8KB of on-cartridge RAM
            break;  
        default:
            perror("bootloader: Unsupported cartridge type:");
            perror((char[]){cart_type_read.byte, '\0'});
            free(gb->rom);
            free(gb);
            return NULL;
    }

    // Now we need to get the size of the on-cartridge RAM in banks, this is in ROM addr 0x0149
    // TODO: make all of these special addresses into defines
    gb_read_byte_t cart_ram_size_read = gb_rom_read(gb, 0x0149);
    if(!cart_ram_size_read.valid) {
        perror("bootloader: Failed to read cartridge RAM size from ROM");
        free(gb->rom);
        free(gb);
        return NULL;
    }
    // TODO: put the tables from the manual here and in the other switch statements for sizes
    /* 
     * From the GBCPUManual section 2.54 page 12
     * RAM size:
     *  0 - None
     *  1 - 16kBit  = 2kB   = 1 bank
     *  2 - 64kBit  = 8kB   = 1 bank
     *  3 - 256kBit = 32kB  = 4 banks
     *  4 - 1MBit   = 128kB = 16 banks
     */
    switch (cart_ram_size_read.byte) {
        case 0x00: // No RAM
            gb->num_cart_ram_banks = 0;
            break;
        case 0x01: // 2KB RAM
            gb->num_cart_ram_banks = 1; // No block switching, must be 2KB of on-cartridge RAM
            break;
        case 0x02: // 8KB RAM
            gb->num_cart_ram_banks = 1; // No block switching, must be 8KB of on-cartridge RAM
            break;
        case 0x03: // 32KB RAM (4 banks of 8KB each)
            gb->num_cart_ram_banks = 4;
            break;
        case 0x04: // 128KB RAM (16 banks of 8KB each)
            gb->num_cart_ram_banks = 16;
            break;
        case 0x05: // 64KB RAM (8 banks of 8KB each)
            gb->num_cart_ram_banks = 8;
            break;  
        default:
            perror("bootloader: Unsupported cartridge RAM size:");
            perror((char[]){cart_ram_size_read.byte, '\0'});
            free(gb->rom);
            free(gb);
            return NULL;
    }


    // Assign the function pointers in the gb_s struct to the defined functions

    return gb;
}
