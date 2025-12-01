
/*
 * Bootloader implementation for Game Boy Emulator
 * Author: Dan Renardson
 * Date: 2025-10-20
 * 
 * Modified By: Noah Kremler
 * Date: 2025-11-06 
 *
 * This file contains the bootloader code responsible for initializing
 * the emulator and loading the game ROM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "rom.h"
#include "gb_types.h"
#include "memory.h"
#include "cpu.h"



// ROM header addresses
// These mark the start and end locations of specific metadata fields 
//   and validation assets that the Game Boy hardware and BIOS use to 
//   identify, validate, and configure a game cartridge when booting.
#define ROM_HEADER_TITLE_START      0x0134
#define ROM_HEADER_TITLE_END        0x0143
#define ROM_HEADER_CGB_FLAG         0x0143
#define ROM_HEADER_SGB_FLAG         0x0146
#define ROM_HEADER_CART_TYPE        0x0147
#define ROM_HEADER_ROM_SIZE         0x0148
#define ROM_HEADER_RAM_SIZE         0x0149
#define ROM_HEADER_CHECKSUM         0x014D

#define NINTENDO_LOGO_START         0x0104
#define NINTENDO_LOGO_END           0x0133

// Global ROM storage (will be accessed via callbacks)
static uint8_t *g_rom_data = NULL;
static size_t g_rom_size = 0;

static uint8_t *g_cart_ram = NULL;
static size_t g_cart_ram_size = 0;


// -------------------------------
// DAN's FUNCTIONS
// - I have refactored the bootloader code, doing my best to keep Dan's documentation and outputs the same as before.
// - I have done it this way in order to keep the cpu and memory code fully functional.
// -------------------------------

// gb_read_byte_t gb_rom_read(struct gb_s* gb, uint16_t addr) {

//     // Fail condition return values
//     gb_read_byte_t result = {0, false};

//     if(!gb) {
//         perror("gb_rom_read: NULL gb_s pointer");
//         return result;
//     }
//     if(!gb->rom) {
//         perror("gb_rom_read: NULL ROM pointer in gb_s struct");
//         return result;
//     }

//     // if an attempt is made to read from ROM that is at address >= 0x3FFF then that's an error automatically
//     // [0x0000, 0x1FFF] => Block 0
//     // [0x2000, 0x3FFF] => Selected Block
//     if(addr >= (2*ROM_BANK_SIZE)-1) {
//         perror("gb_rom_read: Address out of range, addr = ");
//         perror((char[]){(char)(addr >> 8), (char)(addr & 0xFF), '\0'});
//         return result;
//     }

//     uint16_t rom_array_addr = 0;

//     // Calculate the actual address in the ROM array
//     if(addr < ROM_BANK_SIZE) {
//         // Address is in fixed bank 0
//         rom_array_addr = (size_t)(addr);
//     }
//     else {
//         // Address is in the switchable bank
//         rom_array_addr = (size_t)(gb->selected_rom_bank * ROM_BANK_SIZE + addr);
//     }
    
//     // Return the byte at the address of the entire ROM block
//     result.byte = gb->rom[rom_array_addr];
//     result.valid = true;
//     return result;
// }


// bool gb_rom_select_bank(struct gb_s* gb, uint8_t bank) {

//     if(!gb) {
//         perror("gb_rom_select_bank: NULL gb_s pointer");
//         return false;
//     }

//     if(!gb->mbc) {
//         perror("gb_rom_select_bank: Cartridge has no MBC, cannot select bank = ");
//         perror((char[]){(char)bank, '\0'});
//         return false;
//     }

//     // Bank 0 is always available in addresses [0x0000, 0x1FFF], so selecting it is not allowed
//     if(bank == 0) {
//         perror("gb_rom_select_bank: Cannot select bank 0, remapping to bank 1\n");
//         bank = 1;   
//     }

//     // This checks whether the caller is attempting to select a block that is too high to exist on any cartrige
//     // Later we check against the actual loaded ROM size
//     if(bank >= gb->num_rom_banks) {
//         perror("gb_rom_select_bank: Bank number out of range, bank = ");
//         perror((char[]){(char)bank, '\0'});
//         perror(" , max banks = ");
//         perror((char[]){(char)(gb->num_rom_banks), '\0'});
//         return false;
//     }

//     gb->selected_rom_bank = bank;
//     return false;
// }


// -------------------------------
// ROM Callback Functions
// -------------------------------

// ROM read callback - accessed by MMU
uint8_t bootloader_rom_read(struct gb_s *gb, uint32_t addr) {
    (void)gb; // Unused parameter
    
    if (addr < g_rom_size) {
        return g_rom_data[addr];
    }
    return 0xFF;
}


// Cart RAM read callback
uint8_t bootloader_cart_ram_read(struct gb_s *gb, uint32_t addr) {
    (void)gb; // Unused parameter
    
    if (g_cart_ram && addr < g_cart_ram_size) {
        return g_cart_ram[addr];
    }
    return 0xFF;
}


// Cart RAM write callback
void bootloader_cart_ram_write(struct gb_s *gb, uint32_t addr, uint8_t val) {
    (void)gb; /* Unused */
    
    if (g_cart_ram && addr < g_cart_ram_size) {
        g_cart_ram[addr] = val;
    }
}


// Error handler callback
void bootloader_error_handler(struct gb_s *gb, enum gb_error_e error, uint16_t addr) {
    const char *error_str[] = {
        "No error",
        "Invalid opcode",
        "Invalid read",
        "Invalid write"
    };
    
    fprintf(stderr, "EMULATOR ERROR: %s at address 0x%04X\n",
            error < GB_INVALID_MAX ? error_str[error] : "Unknown error",
            addr);
    fprintf(stderr, "PC: 0x%04X, A: 0x%02X, OpCode: 0x%02X\n",
            gb->cpu_reg.pc.reg, gb->cpu_reg.a, mmu_read(gb, addr));
    
    /* Halt execution */
    exit(1);
}


// -------------------------------
// Helper Functions
// -------------------------------

// Verify the scrolling Nintendo graphic as a sanity check
static bool verify_nintendo_logo(void) {
    
    static const uint8_t correct_nintendo_graphic[] = {
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
        0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
    };
    
    // Loop through each of the the Nintendo graphic bytes and compare to the correct
    for(size_t i = 0; i < sizeof(correct_nintendo_graphic); i++) {
        if(g_rom_data[NINTENDO_LOGO_START + i] != correct_nintendo_graphic[i]) {
            printf("bootloader: Nintendo graphic mismatch at address 0x%04X, expected 0x%02X, got 0x%02X\n",
                NINTENDO_LOGO_START + (int)i, correct_nintendo_graphic[i], g_rom_data[NINTENDO_LOGO_START + i]);
            return false;
        }
    }

    printf("bootloader: Successfully verified Nintendo graphic in ROM\n");
    return true;
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
static uint8_t get_num_rom_banks(uint8_t size_code) {

    uint8_t num_rom_banks = 0;
    switch (size_code) {
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
            printf("bootloader: Unsupported ROM size code: 0x%02X\n", size_code);
            return 0;
    }

    printf("bootloader: Detected ROM size code: %02X, num banks: %d\n", size_code, num_rom_banks);
    return num_rom_banks;
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
static uint8_t get_num_ram_banks(uint8_t size_code) {

    uint8_t num_cart_ram_banks = 0;
    switch (size_code) {
        case 0x00: // No RAM
            num_cart_ram_banks = 0;
            break;
        case 0x01: // 2KB RAM
            num_cart_ram_banks = 1; // Special case for 2 kB on-cart RAM - treat as 1 bank
            break;
        case 0x02: // 8KB RAM
            num_cart_ram_banks = 1; // No block switching, must be 8KB of on-cartridge RAM
            break;
        case 0x03: // 32KB RAM (4 banks of 8KB each)
            num_cart_ram_banks = 4;
            break;
        case 0x04: // 128KB RAM (16 banks of 8KB each)
            num_cart_ram_banks = 16;
            break;
        case 0x05: // 64KB RAM (8 banks of 8KB each)
            num_cart_ram_banks = 8;
            break;  
        default:
            printf("bootloader: Unsupported cartridge RAM size: 0x%02X\n", size_code);
            return 0;
    }

    return num_cart_ram_banks;
}


/**
 * 0x0147 Cartridge type (hex):
 * 0x00 - ROM ONLY 
 * 0x01 - ROM + MBC1
 * 0x02 - ROM + MBC1 + RAM
 * 0x03 - ROM + MBC1 + RAM + BATTERY
 * 0x05 - ROM + MBC2
 * 0x06 - ROM + MBC2 + BATTERY
 * 0x08 - ROM + RAM
 * 0x09 - ROM + RAM + BATTERY
 */
static int8_t get_mbc_type(uint8_t cart_type) {
    switch (cart_type) {
        case 0x00: // No MBC + no cartridge RAM
            return 0;
        case 0x01: // MBC1 + not cartridge RAM
        case 0x02: // MBC1 + RAM
        case 0x03: // MBC1 + RAM + Battery (same as 0x02 for our purposes)
            return 1;
        case 0x08: // No MBC + RAM 
        case 0x09: // No MBC + RAM + Battery (same as 0x08 for our purposes)
            return 0;
        default:
            printf("bootloader: Unsupported cartridge type: 0x%2X\n", cart_type);
            return -1;
    }
}

// Check if cartridge has RAM
static bool has_cart_ram(uint8_t cart_type) {
    switch (cart_type) {
        case 0x02: // ROM + MBC1 + RAM
        case 0x03: // ROM + MBC1 + RAM + BATTERY
        case 0x08: // ROM + RAM
        case 0x09: // ROM + RAM + BATTERY
            return true;
        default:
            return false;
    }
}

// Print ROM title from header
static void print_rom_title(void) {
    printf("Welcome to ");
    for (uint16_t i = ROM_HEADER_TITLE_START; i <= ROM_HEADER_TITLE_END; i++) {
        char c = (char)g_rom_data[i];
        if (c >= 0x20 && c <= 0x7E) {  /* Printable ASCII */
            printf("%c", c);
        } else if (c == 0) {
            break;
        }
    }
    printf("\n");
}


// -------------------------------
// Main Bootloader Function
// -------------------------------

// Load ROM file and initialize emulator
struct gb_s* bootloader(char* rom_path) {

    FILE *rom_file = NULL;
    struct gb_s *gb = NULL;

    printf("=== Game Boy Emulator Bootloader ===\n");
    printf("bootloader: Loading ROM: %s\n", rom_path);

    // Open the ROM file
    rom_file = fopen(rom_path, "rb");
    if (!rom_file) {
        perror("bootloader: Failed to open ROM file at:");
        perror(rom_path);
        return NULL;
    }

    // Get file size
    fseek(rom_file, 0, SEEK_END);
    g_rom_size = ftell(rom_file);
    fseek(rom_file, 0, SEEK_SET);
    
    printf("bootloader: ROM file size: %zu bytes\n", g_rom_size);

    // Allocate ROM memory
    g_rom_data = (uint8_t*)malloc(g_rom_size);
    if (!g_rom_data) {
        perror("bootloader: Failed to allocate memory for ROM");
        fclose(rom_file);
        return NULL;
    }

    // Read ROM data
    if (fread(g_rom_data, 1, g_rom_size, rom_file) != g_rom_size) {
        fprintf(stderr, "bootloader: Failed to read ROM data\n");
        free(g_rom_data);
        g_rom_data = NULL;
        fclose(rom_file);
        return NULL;
    }
    
    fclose(rom_file);
    
    // Verify Nintendo logo
    if (!verify_nintendo_logo()) {
        fprintf(stderr, "bootloader: Nintendo logo verification failed\n");
        free(g_rom_data);
        g_rom_data = NULL;
        return NULL;
    }
    printf("bootloader: Nintendo logo verified\n");
    
    // Check for super GameBoy cartridge, unsupported
    if (g_rom_data[ROM_HEADER_SGB_FLAG] == 0x03) {
        printf("bootloader: Super GameBoy cartridges are unsupported\n");
        free(g_rom_data);
        g_rom_data = NULL;
        return NULL;
    }

    // Check for GameBoy Color cartridge but don't reject, just warn
    // Rejecting is too strict since many games are dual-compatible
    if (g_rom_data[ROM_HEADER_CGB_FLAG] & 0x80) {
        printf("bootloader: CGB-compatible ROM detected (running in DMG mode)\n");
    }

    // Parse ROM header
    uint8_t rom_size_code = g_rom_data[ROM_HEADER_ROM_SIZE];
    uint8_t ram_size_code = g_rom_data[ROM_HEADER_RAM_SIZE];
    uint8_t cart_type = g_rom_data[ROM_HEADER_CART_TYPE];
    
    uint8_t num_rom_banks = get_num_rom_banks(rom_size_code);
    uint8_t num_ram_banks = get_num_ram_banks(ram_size_code);
    int8_t mbc_type = get_mbc_type(cart_type);
    
    if (num_rom_banks == 0) {
        fprintf(stderr, "bootloader: Invalid ROM size\n");
        free(g_rom_data);
        g_rom_data = NULL;
        return NULL;
    }
    
    if (mbc_type < 0) {
        fprintf(stderr, "bootloader: Unsupported cartridge type: 0x%02X\n", cart_type);
        free(g_rom_data);
        g_rom_data = NULL;
        return NULL;
    }
    
    if (mbc_type != 0 && mbc_type != 1) {
        fprintf(stderr, "bootloader: Only MBC1 is supported (got MBC%d)\n", mbc_type);
        free(g_rom_data);
        g_rom_data = NULL;
        return NULL;
    }
    
    printf("bootloader: Cartridge type: 0x%02X (MBC%d)\n", cart_type, mbc_type);
    printf("bootloader: ROM banks: %d (%d KB)\n", num_rom_banks, (num_rom_banks * ROM_BANK_SIZE) / 1024);
    printf("bootloader: RAM banks: %d (%d KB)\n", num_ram_banks, (num_ram_banks * CRAM_BANK_SIZE) / 1024);
    
    // Allocate cart RAM if needed
    if (has_cart_ram(cart_type) && num_ram_banks > 0) {
        g_cart_ram_size = num_ram_banks * CRAM_BANK_SIZE;
        g_cart_ram = (uint8_t*)calloc(1, g_cart_ram_size);
        if (!g_cart_ram) {
            fprintf(stderr, "bootloader: Failed to allocate cart RAM\n");
            free(g_rom_data);
            g_rom_data = NULL;
            return NULL;
        }
        printf("bootloader: Allocated %zu bytes for cart RAM\n", g_cart_ram_size);
    }
    
    // Allocate and initialize emulator context
    gb = (struct gb_s*)calloc(1, sizeof(struct gb_s));
    if (!gb) {
        fprintf(stderr, "bootloader: Failed to allocate emulator context\n");
        free(g_rom_data);
        free(g_cart_ram);
        g_rom_data = NULL;
        g_cart_ram = NULL;
        return NULL;
    }
    
    // Set up callbacks
    gb->gb_rom_read = bootloader_rom_read;
    gb->gb_cart_ram_read = bootloader_cart_ram_read;
    gb->gb_cart_ram_write = bootloader_cart_ram_write;
    gb->gb_error = bootloader_error_handler;
    
    // Set cartridge info
    gb->mbc = mbc_type;
    gb->cart_ram = has_cart_ram(cart_type) ? 1 : 0;
    gb->num_rom_banks_mask = num_rom_banks - 1;
    gb->num_ram_banks = num_ram_banks;
    
    // Initialize MMU and CPU
    mmu_init(gb);
    cpu_init(gb);
    
    // Print a welcome message with the name of the game that was loaded form the ROM
    print_rom_title();
    
    // Success!
    printf("bootloader: Successfully loaded ROM and initialized gb_s struct\n");
    printf("====================================\n\n");
    
    return gb;
}


// Clean up ROM and RAM
void bootloader_cleanup(void) {
    
    if (g_rom_data) {
        free(g_rom_data);
        g_rom_data = NULL;
    }
    
    if (g_cart_ram) {
        free(g_cart_ram);
        g_cart_ram = NULL;
    }
}
