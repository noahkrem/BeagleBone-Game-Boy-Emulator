#ifndef REGISTERS_H
#define REGISTERS_H

#include "gb_types.h"

#include <stdint.h>

void regInit();
void regShutdown();

/**
 * loads 8-bit i into register reg
 * register number should be 3 bits
 */
void regSet8(struct gb_s* gb, const uint8_t reg, const uint8_t val);

/**
 * loads 16-bit i into register reg pair
 * register number should be 2 bits
 */
void regSet16(struct gb_s* gb, const uint8_t reg, const uint16_t val);

/**
 * loads 16-bit i into register reg pair
 * register number should be 2 bits
 */
uint16_t regSet16stk(struct gb_s* gb, const uint8_t reg, const uint16_t val);

/**
 * returns the 8-bit content of register reg
 */
uint8_t regGet8(struct gb_s* gb, const uint8_t reg);

/**
 * returns the 16-bit content of register reg pair
 */
uint16_t regGet16(struct gb_s* gb, const uint8_t reg);

/**
 * returns the 16-bit content of register reg pair
 */
uint16_t regGet16stk(struct gb_s* gb, const uint8_t reg);

/**
 * returns the 16-bit content of register reg pair
 */
uint16_t regGet16mem(struct gb_s* gb, const uint8_t reg);

#endif