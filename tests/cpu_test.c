/**
 * cpu_test.c - Simple test program for CPU + MMU
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gb_types.h"
#include "cpu.h"
#include "memory.h"

/* Simple test ROM in memory */
static uint8_t test_rom[0x8000];
static uint8_t test_ram[0x2000];

/* ROM read callback */
uint8_t rom_read(struct gb_s *gb, uint32_t addr) {
    
    (void)gb;  // Unused parameter

    if (addr < sizeof(test_rom)) {
        return test_rom[addr];
    }
    return 0xFF;
}

/* Cart RAM read callback */
uint8_t cart_ram_read(struct gb_s *gb, uint32_t addr) {
    
    (void)gb;  // Unused parameter

    if (addr < sizeof(test_ram)) {
        return test_ram[addr];
    }
    return 0xFF;
}

/* Cart RAM write callback */
void cart_ram_write(struct gb_s *gb, uint32_t addr, uint8_t val) {
    
    (void)gb;  // Unused parameter
    
    if (addr < sizeof(test_ram)) {
        test_ram[addr] = val;
    }
}

/* Error handler */
void error_handler(struct gb_s *gb, enum gb_error_e error, uint16_t addr) {
    const char *error_str[] = {
        "Unknown error",
        "Invalid opcode",
        "Invalid read",
        "Invalid write"
    };
    
    printf("ERROR: %s at address 0x%04X\n", 
           error < GB_INVALID_MAX ? error_str[error] : "Unknown",
           addr);
    printf("PC: 0x%04X, SP: 0x%04X\n", gb->cpu_reg.pc.reg, gb->cpu_reg.sp.reg);
    printf("A: 0x%02X, Flags: Z=%d N=%d H=%d C=%d\n",
           gb->cpu_reg.a,
           gb->cpu_reg.f.f_bits.z,
           gb->cpu_reg.f.f_bits.n,
           gb->cpu_reg.f.f_bits.h,
           gb->cpu_reg.f.f_bits.c);
    exit(1);
}

/* Helper to print CPU state */
void print_cpu_state(struct gb_s *gb) {
    printf("PC:0x%04X SP:0x%04X A:0x%02X BC:0x%04X DE:0x%04X HL:0x%04X F:%c%c%c%c\n",
           gb->cpu_reg.pc.reg,
           gb->cpu_reg.sp.reg,
           gb->cpu_reg.a,
           gb->cpu_reg.bc.reg,
           gb->cpu_reg.de.reg,
           gb->cpu_reg.hl.reg,
           gb->cpu_reg.f.f_bits.z ? 'Z' : '-',
           gb->cpu_reg.f.f_bits.n ? 'N' : '-',
           gb->cpu_reg.f.f_bits.h ? 'H' : '-',
           gb->cpu_reg.f.f_bits.c ? 'C' : '-');
}

/* Test 1: Basic instructions */
void test_basic_instructions(void) {
    printf("\n=== Test 1: Basic Instructions ===\n");
    
    struct gb_s gb = {0};
    
    /* Set up callbacks */
    gb.gb_rom_read = rom_read;
    gb.gb_cart_ram_read = cart_ram_read;
    gb.gb_cart_ram_write = cart_ram_write;
    gb.gb_error = error_handler;
    
    /* Initialize */
    mmu_init(&gb);
    cpu_init(&gb);
    
    /* Create a simple test program */
    test_rom[0x0100] = 0x3E;  /* LD A, 0x42 */
    test_rom[0x0101] = 0x42;
    test_rom[0x0102] = 0x3C;  /* INC A */
    test_rom[0x0103] = 0x3D;  /* DEC A */
    test_rom[0x0104] = 0x3D;  /* DEC A */
    test_rom[0x0105] = 0x76;  /* HALT */
    
    printf("Initial state:\n");
    print_cpu_state(&gb);
    
    /* Execute instructions */
    for (int i = 0; i < 5 && !gb.gb_halt; i++) {
        uint16_t cycles = cpu_step(&gb);
        printf("Step %d (took %d cycles):\n", i + 1, cycles);
        print_cpu_state(&gb);
    }
    
    /* Verify results */
    if (gb.cpu_reg.a == 0x41 && gb.cpu_reg.f.f_bits.z == 0) {
        printf("✓ Test PASSED: A = 0x%02X (expected 0x41)\n", gb.cpu_reg.a);
    } else {
        printf("✗ Test FAILED: A = 0x%02X (expected 0x41)\n", gb.cpu_reg.a);
    }
}

/* Test 2: Arithmetic and flags */
void test_arithmetic_flags(void) {
    printf("\n=== Test 2: Arithmetic and Flags ===\n");
    
    struct gb_s gb = {0};
    gb.gb_rom_read = rom_read;
    gb.gb_cart_ram_read = cart_ram_read;
    gb.gb_cart_ram_write = cart_ram_write;
    gb.gb_error = error_handler;
    
    mmu_init(&gb);
    cpu_init(&gb);
    
    /* Test program: ADD and SUB */
    test_rom[0x0100] = 0x3E;  /* LD A, 0xFF */
    test_rom[0x0101] = 0xFF;
    test_rom[0x0102] = 0xC6;  /* ADD A, 0x01 */
    test_rom[0x0103] = 0x01;
    test_rom[0x0104] = 0x76;  /* HALT */
    
    printf("Testing ADD with overflow:\n");
    print_cpu_state(&gb);
    
    cpu_step(&gb);  /* LD A, 0xFF */
    cpu_step(&gb);  /* ADD A, 0x01 */
    
    print_cpu_state(&gb);
    
    if (gb.cpu_reg.a == 0x00 && 
        gb.cpu_reg.f.f_bits.z == 1 && 
        gb.cpu_reg.f.f_bits.c == 1) {
        printf("✓ Test PASSED: Overflow sets Z and C flags\n");
    } else {
        printf("✗ Test FAILED: Flags incorrect (Z=%d C=%d)\n",
               gb.cpu_reg.f.f_bits.z, gb.cpu_reg.f.f_bits.c);
    }
}

/* Test 3: Memory access */
void test_memory_access(void) {
    printf("\n=== Test 3: Memory Access ===\n");
    
    struct gb_s gb = {0};
    gb.gb_rom_read = rom_read;
    gb.gb_cart_ram_read = cart_ram_read;
    gb.gb_cart_ram_write = cart_ram_write;
    gb.gb_error = error_handler;
    
    mmu_init(&gb);
    cpu_init(&gb);
    
    /* Test program: Write to and read from memory */
    test_rom[0x0100] = 0x21;  /* LD HL, 0xC000 */
    test_rom[0x0101] = 0x00;
    test_rom[0x0102] = 0xC0;
    test_rom[0x0103] = 0x3E;  /* LD A, 0x55 */
    test_rom[0x0104] = 0x55;
    test_rom[0x0105] = 0x77;  /* LD (HL), A */
    test_rom[0x0106] = 0x3E;  /* LD A, 0x00 */
    test_rom[0x0107] = 0x00;
    test_rom[0x0108] = 0x7E;  /* LD A, (HL) */
    test_rom[0x0109] = 0x76;  /* HALT */
    
    printf("Testing memory read/write:\n");
    
    for (int i = 0; i < 6; i++) {
        cpu_step(&gb);
    }
    
    print_cpu_state(&gb);
    
    if (gb.cpu_reg.a == 0x55) {
        printf("✓ Test PASSED: Memory read/write works (A = 0x%02X)\n", gb.cpu_reg.a);
    } else {
        printf("✗ Test FAILED: A = 0x%02X (expected 0x55)\n", gb.cpu_reg.a);
    }
}

/* Test 4: Stack operations */
void test_stack_operations(void) {
    printf("\n=== Test 4: Stack Operations ===\n");
    
    struct gb_s gb = {0};
    gb.gb_rom_read = rom_read;
    gb.gb_cart_ram_read = cart_ram_read;
    gb.gb_cart_ram_write = cart_ram_write;
    gb.gb_error = error_handler;
    
    mmu_init(&gb);
    cpu_init(&gb);
    
    /* Test program: PUSH and POP */
    test_rom[0x0100] = 0x01;  /* LD BC, 0x1234 */
    test_rom[0x0101] = 0x34;
    test_rom[0x0102] = 0x12;
    test_rom[0x0103] = 0xC5;  /* PUSH BC */
    test_rom[0x0104] = 0x01;  /* LD BC, 0x0000 */
    test_rom[0x0105] = 0x00;
    test_rom[0x0106] = 0x00;
    test_rom[0x0107] = 0xC1;  /* POP BC */
    test_rom[0x0108] = 0x76;  /* HALT */
    
    printf("Testing PUSH/POP:\n");
    uint16_t initial_sp = gb.cpu_reg.sp.reg;
    
    for (int i = 0; i < 5; i++) {
        cpu_step(&gb);
    }
    
    print_cpu_state(&gb);
    
    if (gb.cpu_reg.bc.reg == 0x1234 && gb.cpu_reg.sp.reg == initial_sp) {
        printf("✓ Test PASSED: PUSH/POP works (BC = 0x%04X, SP restored)\n", 
               gb.cpu_reg.bc.reg);
    } else {
        printf("✗ Test FAILED: BC = 0x%04X (expected 0x1234)\n", 
               gb.cpu_reg.bc.reg);
    }
}

/* Test 5: Jump instructions */
void test_jumps(void) {
    printf("\n=== Test 5: Jump Instructions ===\n");
    
    struct gb_s gb = {0};
    gb.gb_rom_read = rom_read;
    gb.gb_cart_ram_read = cart_ram_read;
    gb.gb_cart_ram_write = cart_ram_write;
    gb.gb_error = error_handler;
    
    mmu_init(&gb);
    cpu_init(&gb);
    
    /* Test program: Conditional jump */
    test_rom[0x0100] = 0x3E;  /* LD A, 0x00 */
    test_rom[0x0101] = 0x00;
    test_rom[0x0102] = 0xCA;  /* JP Z, 0x0106 */
    test_rom[0x0103] = 0x06;
    test_rom[0x0104] = 0x01;
    test_rom[0x0105] = 0x3C;  /* INC A (should be skipped) */
    test_rom[0x0106] = 0x76;  /* HALT */
    
    printf("Testing conditional jump:\n");
    
    cpu_step(&gb);  /* LD A, 0x00 (sets Z flag) */
    cpu_step(&gb);  /* JP Z, 0x0106 (should jump) */
    
    print_cpu_state(&gb);
    
    if (gb.cpu_reg.pc.reg == 0x0106 && gb.cpu_reg.a == 0x00) {
        printf("✓ Test PASSED: Jump taken (PC = 0x%04X, A not incremented)\n", 
               gb.cpu_reg.pc.reg);
    } else {
        printf("✗ Test FAILED: PC = 0x%04X (expected 0x0106)\n", 
               gb.cpu_reg.pc.reg);
    }
}

int main(void) {
    printf("====================================\n");
    printf("  Game Boy CPU + MMU Test Suite\n");
    printf("====================================\n");
    
    /* Clear test ROM/RAM */
    memset(test_rom, 0, sizeof(test_rom));
    memset(test_ram, 0, sizeof(test_ram));
    
    /* Run tests */
    test_basic_instructions();
    test_arithmetic_flags();
    test_memory_access();
    test_stack_operations();
    test_jumps();
    
    printf("\n====================================\n");
    printf("  All tests completed!\n");
    printf("====================================\n");
    
    return 0;
}