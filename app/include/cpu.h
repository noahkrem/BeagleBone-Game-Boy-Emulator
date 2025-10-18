/**
 * cpu.h - Game Boy (Sharp LR35902) CPU Emulator Header
 * Author: Noah Kremler
 * Date: 2024-06-15
 * 
 * Handles: CPU state, instruction set, and execution logic.
 */


#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>


uint16_t cpu_step();


#endif