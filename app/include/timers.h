/**
 * timers.h - Timer Subsystem Interface
 */

#ifndef GB_TIMERS_H
#define GB_TIMERS_H

#include "gb_types.h"

/**
 * Update timers for given number of CPU cycles
 * 
 * @param gb        Emulator context
 * @param cycles    Number of CPU cycles that have passed
 */
void timers_step(struct gb_s *gb, uint16_t cycles);

/**
 * Reset timers to initial state
 * 
 * @param gb        Emulator context
 */
void timers_reset(struct gb_s *gb);

#endif /* GB_TIMERS_H */