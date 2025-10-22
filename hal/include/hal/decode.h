#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t pc;          // address the opcode was read from
    uint8_t  opcode;      // raw opcode (or CB sub-op if cb == true)
    bool     cb;          // true if this is a CB-prefixed instruction
    uint8_t  length;      // 1, 2, or 3 bytes total (including prefix/opcode)
    uint8_t  cycles_min;  // base cycles
    uint8_t  cycles_max;  // cycles if branch taken/condition true
    char     text[48];    // "LD A,(HL)" or "BIT 7,H", etc.
} GBDecoded;

/**
 * Decode one LR35902 instruction at pc.
 * - fetch_cb must return the byte at (pc + i). You can wire it to your mmu read.
 * - On success, out->length tells how many bytes to advance PC by.
 */
typedef uint8_t (*gb_fetch_fn)(uint16_t addr, void *user);

void gb_decode(gb_fetch_fn fetch, void *user, uint16_t pc, GBDecoded *out);

#ifdef __cplusplus
}
#endif
