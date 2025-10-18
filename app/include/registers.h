#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>

void regInit();
void regShutdown();

/**
 * loads 8-bit i into register reg
 * register number should be 3 bits
 */
void setReg8(uint8_t reg, uint8_t i);

/**
 * loads 16-bit i into register reg pair
 * register number should be 2 bits
 */
void setReg16(uint8_t reg, uint16_t i);

/**
 * copies the 8-bit content from reg2 to reg1
 */
void copyReg(uint8_t reg1, uint8_t reg2);

/**
 * returns the 8-bit content of register reg
 */
uint8_t getReg8(uint8_t reg);

/**
 * returns the 16-bit content of register reg pair
 */
uint16_t getReg16(uint8_t reg);

#endif