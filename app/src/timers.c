/**
 * timers.c - Minimal Timer Implementation
 * 
 * Implements just the DIV register for MVP.
 * Full timer support (TIMA/TMA/TAC) can be added later.
 */

#include "timers.h"
#include "gpu.h"

/**
 * Update timers based on CPU cycles
 * 
 * For MVP, only DIV register is implemented.
 */
void timers_step(struct gb_s *gb, uint16_t cycles) {
    /* Update DIV register */
    gb->counter.div_count += cycles;
    
    while (gb->counter.div_count >= DIV_CYCLES) {
        gb->counter.div_count -= DIV_CYCLES;
        gb->hram_io[IO_DIV]++;
    }
    
    /* TODO: Implement TIMA/TMA/TAC for games that need it */
}

/**
 * Reset timers to initial state
 */
void timers_reset(struct gb_s *gb) {
    gb->counter.div_count = 0;
    gb->hram_io[IO_DIV] = 0;
}