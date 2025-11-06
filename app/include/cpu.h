/**
 * cpu.h - Game Boy (Sharp LR35902) CPU Emulator Header
 * Author: Noah Kremler
 * Date: 2024-06-15
 * 
 * Handles: CPU state, instruction set, and execution logic.
 */


#ifndef CPU_H
#define CPU_H

#include <stdbool.h>

#include "gb_types.h"

/**
 * Execute a single CPU instruction
 * This is the main main CPU execution function. It fetches and executes a single instruction.
 * 
 * @param gb    Emulator context
 * @return      Number of cycles the instruction took
 */
uint16_t cpu_step(struct gb_s* gb);

/**
 * Execute a single CPU instruction from the CB prefix set
 * These are extended instructions accessed via the 0xCB prefix,
 * used for bit manipulation and other operations.
 * 
 * @param gb    Emulator context
 * @return      Number of cycles the instruction took
 */
uint8_t cpu_execute_cb(struct gb_s* gb);

/**
 * Handle interrupts if enabled
 * Checks interrupt flags and jumps to the appropriate interrupt handler if needed.
 * 
 * @param gb    Emulator context
 */
void cpu_handle_interrupts(struct gb_s* gb);

// Depends on Dan's bootloader, will need to be modified later
void cpu_init(struct gb_s* gb);

/**
 * Reset CPU state to initial values
 * Sets all registers and flags to their default power-on state.
 * 
 * @param gb    Emulator context
 */
void cpu_reset(struct gb_s* gb);

// -------------------------------
// CPU Instruction Helper Macros
// - These are only to make the opcode implementation code cleaner
// - The increment functions are from peanut_gb (e.g. PGB_INSTR_INC_R8)
// -------------------------------

// 8-bit register increment
#define CPU_INC_R8(r)                               \
    r++;									        \
	gb->cpu_reg.f.f_bits.h = ((r & 0x0F) == 0x00);	\
	gb->cpu_reg.f.f_bits.n = 0;						\
	gb->cpu_reg.f.f_bits.z = (r == 0x00)

// 8-bit register decrement, updates flags accordingly
#define CPU_DEC_R8(r)                               \
    r--;									        \
    gb->cpu_reg.f.f_bits.h = ((r & 0x0F) == 0x0F);  \
    gb->cpu_reg.f.f_bits.n = 1;                     \
    gb->cpu_reg.f.f_bits.z = (r == 0x00)

// 8-bit addition with carry, updates flags accordingly
#define CPU_ADC_R8(r, cin)                                                  \
    {                                                                       \
        uint16_t temp = gb->cpu_reg.a + r + cin;                            \
        gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;                   \
        gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0;   \
        gb->cpu_reg.f.f_bits.n = 0;                                         \
        gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);                   \
        gb->cpu_reg.a = (temp & 0xFF);                                      \
    }

// 8-bit subtraction with carry, updates flags accordingly
#define CPU_SBC_R8(r, cin)                                                  \
    {                                                                       \
        uint16_t temp = gb->cpu_reg.a - (r + cin);                          \
        gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;                   \
        gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0;   \
        gb->cpu_reg.f.f_bits.n = 1;                                         \
        gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);                   \
        gb->cpu_reg.a = (temp & 0xFF);                                      \
    }

// 8-bit compare, updates flags accordingly
#define CPU_CP_R8(r)                                                        \
    {                                                                       \
        uint16_t temp = gb->cpu_reg.a - r;                                  \
        gb->cpu_reg.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;                   \
        gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.a ^ r ^ temp) & 0x10) > 0;   \
        gb->cpu_reg.f.f_bits.n = 1;                                         \
        gb->cpu_reg.f.f_bits.z = ((temp & 0xFF) == 0x00);                   \
    }

// 8-bit XOR
#define CPU_XOR_R8(r)                                                       \
    gb->cpu_reg.a ^= r;                                                     \
    gb->cpu_reg.f.reg = 0;                                                  \
    gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00)

// 8-bit OR
#define CPU_OR_R8(r)                                                        \
    gb->cpu_reg.a |= r;                                                     \
    gb->cpu_reg.f.reg = 0;                                                  \
    gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00)

// 8-bit AND
#define CPU_AND_R8(r)                                                       \
    gb->cpu_reg.a &= r;                                                     \
    gb->cpu_reg.f.reg = 0;                                                  \
    gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0x00);                       \
    gb->cpu_reg.f.f_bits.h = 1

// Flag access helpers
// Makes it easier to read the CPU's status flags from the "F" (flags) register.
// This register is part of the gb context's cpu_reg structure.
#define CPU_FLAG_Z(gb) ((gb)->cpu_reg.f.f_bits.z)
#define CPU_FLAG_N(gb) ((gb)->cpu_reg.f.f_bits.n)
#define CPU_FLAG_H(gb) ((gb)->cpu_reg.f.f_bits.h)
#define CPU_FLAG_C(gb) ((gb)->cpu_reg.f.f_bits.c)

#endif  // CPU_H