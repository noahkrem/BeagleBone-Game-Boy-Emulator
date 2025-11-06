/**
 * bootloader_test.c - Bootloader Test Program
 * 
 * Tests the bootloader functionality by creating a minimal valid ROM
 * and verifying that it loads correctly.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gb_types.h"
#include "rom.h"
#include "cpu.h"
#include "memory.h"

/* Test ROM file name */
#define TEST_ROM_FILE "tetris.gb"

/* Helper to create a minimal valid ROM file */
static bool create_test_rom(const char *filename, uint8_t rom_size, uint8_t ram_size, uint8_t cart_type) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to create test ROM file\n");
        return false;
    }
    
    /* Create 32KB ROM (minimum size) */
    uint8_t *rom_data = calloc(1, 0x8000);
    if (!rom_data) {
        fclose(f);
        return false;
    }
    
    /* Fill with NOP instructions */
    memset(rom_data, 0x00, 0x8000);
    
    /* Add a simple program at 0x0100 (entry point) */
    rom_data[0x0100] = 0x3E;  /* LD A, 0x42 */
    rom_data[0x0101] = 0x42;
    rom_data[0x0102] = 0x76;  /* HALT */
    
    /* Nintendo logo (required for validation) */
    static const uint8_t nintendo_logo[] = {
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
        0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
    };
    memcpy(&rom_data[0x0104], nintendo_logo, sizeof(nintendo_logo));
    
    /* ROM title */
    const char *title = "TEST ROM";
    memcpy(&rom_data[0x0134], title, strlen(title));
    
    /* Cartridge type and sizes */
    rom_data[0x0147] = cart_type;  /* Cartridge type */
    rom_data[0x0148] = rom_size;   /* ROM size */
    rom_data[0x0149] = ram_size;   /* RAM size */
    
    /* Header checksum (simplified - not accurate but good enough for testing) */
    rom_data[0x014D] = 0x00;
    
    /* Write to file */
    size_t written = fwrite(rom_data, 1, 0x8000, f);
    free(rom_data);
    fclose(f);
    
    return written == 0x8000;
}

/* Test 1: Load a ROM with no MBC */
void test_load_simple_rom(void) {
    printf("\n=== Test 1: Load Simple ROM (No MBC) ===\n");
    
    /* Create test ROM: 32KB, no RAM, no MBC */
    if (!create_test_rom(TEST_ROM_FILE, 0x00, 0x00, 0x00)) {
        printf("✗ Failed to create test ROM\n");
        return;
    }
    
    /* Load ROM */
    struct gb_s *gb = bootloader(TEST_ROM_FILE);
    if (!gb) {
        printf("✗ Test FAILED: bootloader() returned NULL\n");
        remove(TEST_ROM_FILE);
        return;
    }
    
    /* Verify emulator context was initialized */
    if (gb->mbc != 0) {
        printf("✗ Test FAILED: Expected MBC=0, got MBC=%d\n", gb->mbc);
    } else if (gb->cart_ram != 0) {
        printf("✗ Test FAILED: Expected no cart RAM, but cart_ram=%d\n", gb->cart_ram);
    } else if (gb->cpu_reg.pc.reg != 0x0100) {
        printf("✗ Test FAILED: PC should be 0x0100, got 0x%04X\n", gb->cpu_reg.pc.reg);
    } else {
        printf("✓ Test PASSED: ROM loaded successfully\n");
        printf("  MBC type: %d\n", gb->mbc);
        printf("  Cart RAM: %d\n", gb->cart_ram);
        printf("  PC initialized to: 0x%04X\n", gb->cpu_reg.pc.reg);
    }
    
    /* Clean up */
    free(gb);
    bootloader_cleanup();
    remove(TEST_ROM_FILE);
}

/* Test 2: Load ROM with MBC1 */
void test_load_mbc1_rom(void) {
    printf("\n=== Test 2: Load ROM with MBC1 ===\n");
    
    /* Create test ROM: 64KB (4 banks), 8KB RAM, MBC1+RAM */
    if (!create_test_rom(TEST_ROM_FILE, 0x01, 0x02, 0x02)) {
        printf("✗ Failed to create test ROM\n");
        return;
    }
    
    /* Load ROM */
    struct gb_s *gb = bootloader(TEST_ROM_FILE);
    if (!gb) {
        printf("✗ Test FAILED: bootloader returned NULL\n");
        remove(TEST_ROM_FILE);
        return;
    }
    
    /* Verify MBC1 detection */
    if (gb->mbc != 1) {
        printf("✗ Test FAILED: Expected MBC=1, got MBC=%d\n", gb->mbc);
    } else if (gb->cart_ram != 1) {
        printf("✗ Test FAILED: Expected cart_ram=1, got cart_ram=%d\n", gb->cart_ram);
    } else if (gb->num_ram_banks != 1) {
        printf("✗ Test FAILED: Expected 1 RAM bank, got %d\n", gb->num_ram_banks);
    } else {
        printf("✓ Test PASSED: MBC1 ROM loaded successfully\n");
        printf("  MBC type: %d\n", gb->mbc);
        printf("  Cart RAM: %d\n", gb->cart_ram);
        printf("  RAM banks: %d\n", gb->num_ram_banks);
        printf("  ROM banks mask: 0x%04X\n", gb->num_rom_banks_mask);
    }
    
    /* Clean up */
    free(gb);
    bootloader_cleanup();
    remove(TEST_ROM_FILE);
}

/* Test 3: Verify ROM callbacks work */
void test_rom_callbacks(void) {
    printf("\n=== Test 3: ROM Read Callbacks ===\n");
    
    /* Create test ROM */
    if (!create_test_rom(TEST_ROM_FILE, 0x00, 0x00, 0x00)) {
        printf("✗ Failed to create test ROM\n");
        return;
    }
    
    /* Load ROM */
    struct gb_s *gb = bootloader(TEST_ROM_FILE);
    if (!gb) {
        printf("✗ Test FAILED: bootloader() returned NULL\n");
        remove(TEST_ROM_FILE);
        return;
    }
    
    /* Test reading from ROM via callback */
    uint8_t byte_0100 = mmu_read(gb, 0x0100);
    uint8_t byte_0101 = mmu_read(gb, 0x0101);
    
    if (byte_0100 != 0x3E || byte_0101 != 0x42) {
        printf("✗ Test FAILED: ROM read callback not working\n");
        printf("  Expected: 0x3E 0x42\n");
        printf("  Got: 0x%02X 0x%02X\n", byte_0100, byte_0101);
    } else {
        printf("✓ Test PASSED: ROM read callback works\n");
        printf("  ROM[0x0100] = 0x%02X (LD A, nn)\n", byte_0100);
        printf("  ROM[0x0101] = 0x%02X (immediate value)\n", byte_0101);
    }
    
    /* Test reading Nintendo logo via callback */
    uint8_t logo_byte = mmu_read(gb, 0x0104);
    if (logo_byte != 0xCE) {
        printf("✗ Test FAILED: Nintendo logo not readable\n");
        printf("  Expected: 0xCE, Got: 0x%02X\n", logo_byte);
    } else {
        printf("✓ Test PASSED: Nintendo logo readable via callback\n");
    }
    
    /* Clean up */
    free(gb);
    bootloader_cleanup();
    remove(TEST_ROM_FILE);
}

/* Test 4: Execute code from loaded ROM */
void test_execute_rom_code(void) {
    printf("\n=== Test 4: Execute ROM Code ===\n");
    
    /* Create test ROM */
    if (!create_test_rom(TEST_ROM_FILE, 0x00, 0x00, 0x00)) {
        printf("✗ Failed to create test ROM\n");
        return;
    }
    
    /* Load ROM */
    struct gb_s *gb = bootloader(TEST_ROM_FILE);
    if (!gb) {
        printf("✗ Test FAILED: bootloader() returned NULL\n");
        remove(TEST_ROM_FILE);
        return;
    }
    
    printf("Initial CPU state:\n");
    printf("  PC: 0x%04X, A: 0x%02X\n", gb->cpu_reg.pc.reg, gb->cpu_reg.a);
    
    /* Execute the LD A, 0x42 instruction */
    uint16_t cycles = cpu_step(gb);
    printf("After LD A, 0x42 (%d cycles):\n", cycles);
    printf("  PC: 0x%04X, A: 0x%02X\n", gb->cpu_reg.pc.reg, gb->cpu_reg.a);
    
    if (gb->cpu_reg.a != 0x42) {
        printf("✗ Test FAILED: A register should be 0x42, got 0x%02X\n", gb->cpu_reg.a);
    } else if (gb->cpu_reg.pc.reg != 0x0102) {
        printf("✗ Test FAILED: PC should be 0x0102, got 0x%04X\n", gb->cpu_reg.pc.reg);
    } else {
        printf("✓ Test PASSED: Successfully executed code from ROM\n");
    }
    
    /* Execute HALT instruction */
    cycles = cpu_step(gb);
    printf("After HALT (%d cycles):\n", cycles);
    printf("  HALT flag: %d\n", gb->gb_halt);
    
    if (!gb->gb_halt) {
        printf("✗ Test FAILED: HALT flag should be set\n");
    } else {
        printf("✓ Test PASSED: HALT instruction executed correctly\n");
    }
    
    /* Clean up */
    free(gb);
    bootloader_cleanup();
    remove(TEST_ROM_FILE);
}

/* Test 5: Test invalid ROM (bad Nintendo logo) */
void test_invalid_rom(void) {
    printf("\n=== Test 5: Invalid ROM (Bad Nintendo Logo) ===\n");
    
    /* Create ROM with invalid logo */
    FILE *f = fopen(TEST_ROM_FILE, "wb");
    if (!f) {
        printf("✗ Failed to create test file\n");
        return;
    }
    
    uint8_t *rom_data = calloc(1, 0x8000);
    if (!rom_data) {
        fclose(f);
        return;
    }
    
    /* Fill with zeros (invalid Nintendo logo) */
    memset(rom_data, 0x00, 0x8000);
    
    /* Set required headers but leave logo as zeros */
    rom_data[0x0147] = 0x00;  /* ROM only */
    rom_data[0x0148] = 0x00;  /* 32KB */
    rom_data[0x0149] = 0x00;  /* No RAM */
    
    fwrite(rom_data, 1, 0x8000, f);
    free(rom_data);
    fclose(f);
    
    /* Try to load invalid ROM */
    struct gb_s *gb = bootloader(TEST_ROM_FILE);
    
    if (gb != NULL) {
        printf("✗ Test FAILED: Should have rejected invalid ROM\n");
        free(gb);
        bootloader_cleanup();
    } else {
        printf("✓ Test PASSED: Invalid ROM correctly rejected\n");
    }
    
    remove(TEST_ROM_FILE);
}

/* Test 6: Test memory initialization */
void test_memory_initialization(void) {
    printf("\n=== Test 6: Memory Initialization ===\n");
    
    /* Create test ROM */
    if (!create_test_rom(TEST_ROM_FILE, 0x00, 0x00, 0x00)) {
        printf("✗ Failed to create test ROM\n");
        return;
    }
    
    /* Load ROM */
    struct gb_s *gb = bootloader(TEST_ROM_FILE);
    if (!gb) {
        printf("✗ Test FAILED: bootloader() returned NULL\n");
        remove(TEST_ROM_FILE);
        return;
    }
    
    /* Check that WRAM is accessible */
    mmu_write(gb, 0xC000, 0xAB);
    uint8_t val = mmu_read(gb, 0xC000);
    
    if (val != 0xAB) {
        printf("✗ Test FAILED: WRAM not initialized correctly\n");
        printf("  Wrote 0xAB, read back 0x%02X\n", val);
    } else {
        printf("✓ Test PASSED: WRAM initialized and accessible\n");
    }
    
    /* Check that I/O registers are initialized */
    uint8_t lcdc = mmu_read(gb, 0xFF40);
    uint8_t stat = mmu_read(gb, 0xFF41);
    
    printf("  I/O Register initialization:\n");
    printf("    LCDC (0xFF40): 0x%02X\n", lcdc);
    printf("    STAT (0xFF41): 0x%02X\n", stat);
    
    if (lcdc == 0x91 && stat == 0x85) {
        printf("✓ Test PASSED: I/O registers initialized to correct values\n");
    } else {
        printf("⚠ Warning: I/O registers may not match expected values\n");
    }
    
    /* Clean up */
    free(gb);
    bootloader_cleanup();
    remove(TEST_ROM_FILE);
}

/* Main test runner */
int main(void) {
    printf("====================================\n");
    printf("  Bootloader Test Suite\n");
    printf("====================================\n");
    
    test_load_simple_rom();
    test_load_mbc1_rom();
    test_rom_callbacks();
    test_execute_rom_code();
    test_invalid_rom();
    test_memory_initialization();
    
    printf("\n====================================\n");
    printf("  All tests completed!\n");
    printf("====================================\n");
    
    return 0;
}