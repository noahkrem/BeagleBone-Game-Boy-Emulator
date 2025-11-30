/**
 * cpu.c - Game Boy (Sharp LR35902) CPU Emulator 
 * Author: Noah Kremler & Andrew _
 * Date: 2025-11-03
 * 
 * Handles: CPU state, instruction set, and execution logic.
 * Note: This code has been inspired by Peanut GB.
 */

#include "cpu.h"
#include "registers.h"
#include "gb_types.h"
#include "memory.h"
#include "gpu.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t instr_debug = 0;

// Cycle counts for each opcode (0x00-0xFF)
// Directly mirrors the timing of the original hardware.
// This is important for things like game logic synchronization:
//   Many games rely on specific timing (for interrupts, graphics refreshes, etc.), 
//   so running instructions too fast or slow would break gameplay, cause glitches, 
//   or make the emulator incompatible.
// Usage: When the emulator fetches and executes an opcode, it looks up the number 
//   of cycles required using OPCODE_CYCLES[opcode] and advances the emulation 
//   clock by that value.
static const uint8_t OPCODE_CYCLES[256] = {
    4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,  /* 0x00-0x0F */
    4,12, 8, 8, 4, 4, 8, 4,12, 8, 8, 8, 4, 4, 8, 4,  /* 0x10-0x1F */
    8,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,  /* 0x20-0x2F */
    8,12, 8, 8,12,12,12, 4, 8, 8, 8, 8, 4, 4, 8, 4,  /* 0x30-0x3F */
    4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,  /* 0x40-0x4F */
    4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,  /* 0x50-0x5F */
    4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,  /* 0x60-0x6F */
    8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4,  /* 0x70-0x7F */
    4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,  /* 0x80-0x8F */
    4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,  /* 0x90-0x9F */
    4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,  /* 0xA0-0xAF */
    4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,  /* 0xB0-0xBF */
    8,12,12,16,12,16, 8,16, 8,16,12, 8,12,24, 8,16,  /* 0xC0-0xCF */
    8,12,12, 0,12,16, 8,16, 8,16,12, 0,12, 0, 8,16,  /* 0xD0-0xDF */
   12,12, 8, 0, 0,16, 8,16,16, 4,16, 0, 0, 0, 8,16,  /* 0xE0-0xEF */
   12,12, 8, 4, 0,16, 8,16,12, 8,16, 4, 0, 0, 8,16   /* 0xF0-0xFF */
};


// -------------------------------
// Interrupt Handling
// -------------------------------

void cpu_handle_interrupts(struct gb_s* gb) {
    
    // Check if interrupts are enabled and if any are pending
    if (!gb->gb_ime)
        return;
    
    uint8_t if_flag = gb->hram_io[IO_IF];
    uint8_t ie_flag = gb->hram_io[IO_IE];
    uint8_t interrupts = if_flag & ie_flag & 0x1F;
    
    if (interrupts == 0)
        return;
    
    // Disable interrupts
    gb->gb_ime = false;
    
    // Push PC onto stack
    mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
    mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
    
    // Jump to interrupt handler based on priority
    if (interrupts & 0x01) {            // VBLANK
        static int vblank_count = 0;
        if (vblank_count < 10) {
            printf("DEBUG: VBLANK interrupt fired! count=%d\n", vblank_count++);
        }
        gb->cpu_reg.pc.reg = 0x0040;
        gb->hram_io[IO_IF] &= ~0x01;
    } else if (interrupts & 0x02) {     // LCD STATs
        gb->cpu_reg.pc.reg = 0x0048;
        gb->hram_io[IO_IF] &= ~0x02;
    } else if (interrupts & 0x04) {     // TIMER
        gb->cpu_reg.pc.reg = 0x0050;
        gb->hram_io[IO_IF] &= ~0x04;
    } else if (interrupts & 0x08) {     // SERIAL 
        gb->cpu_reg.pc.reg = 0x0058;
        gb->hram_io[IO_IF] &= ~0x08;
    } else if (interrupts & 0x10) {     // JOYPAD
        gb->cpu_reg.pc.reg = 0x0060;
        gb->hram_io[IO_IF] &= ~0x10;
    }
}

// -------------------------------
// CB-Prefixed Instructions
// - CB instructions have an extended instruction set, with the 0xCB opcode reserved 
//     for a second set of 256 instructions (0xCB00–0xCBFF).
// - CB-prefixed instructions warrant a dedicated function because they represent a 
//     logically separate instruction family with distinct semantics.
// -------------------------------

uint8_t cpu_execute_cb(struct gb_s *gb) {
    uint8_t cycles = 8;
    uint8_t cbop = mmu_read(gb, gb->cpu_reg.pc.reg++);
    uint8_t reg_idx = cbop & 0x7;
    uint8_t bit = (cbop >> 3) & 0x7;
    uint8_t op_type = cbop >> 6;
    uint8_t val;
    bool writeback = true;
    
    /* Get value from register or (HL) */
    switch (reg_idx) {
        case 0: val = gb->cpu_reg.bc.bytes.b; break;
        case 1: val = gb->cpu_reg.bc.bytes.c; break;
        case 2: val = gb->cpu_reg.de.bytes.d; break;
        case 3: val = gb->cpu_reg.de.bytes.e; break;
        case 4: val = gb->cpu_reg.hl.bytes.h; break;
        case 5: val = gb->cpu_reg.hl.bytes.l; break;
        case 6: 
            val = mmu_read(gb, gb->cpu_reg.hl.reg);
            cycles = 16; /* (HL) operations take longer */
            break;
        case 7: val = gb->cpu_reg.a; break;
    }
    
    /* Execute operation based on type */
    switch (op_type) {
        case 0: /* Rotates and shifts */
        {
            uint8_t op = (cbop >> 3) & 0x7;
            switch (op) {
                case 0: /* RLC */
                    gb->cpu_reg.f.reg = 0;
                    gb->cpu_reg.f.f_bits.c = (val >> 7);
                    val = (val << 1) | (val >> 7);
                    gb->cpu_reg.f.f_bits.z = (val == 0);
                    break;
                case 1: /* RRC */
                    gb->cpu_reg.f.reg = 0;
                    gb->cpu_reg.f.f_bits.c = val & 1;
                    val = (val >> 1) | (val << 7);
                    gb->cpu_reg.f.f_bits.z = (val == 0);
                    break;
                case 2: /* RL */
                {
                    uint8_t carry = gb->cpu_reg.f.f_bits.c;
                    gb->cpu_reg.f.reg = 0;
                    gb->cpu_reg.f.f_bits.c = (val >> 7);
                    val = (val << 1) | carry;
                    gb->cpu_reg.f.f_bits.z = (val == 0);
                    break;
                }
                case 3: /* RR */
                {
                    uint8_t carry = gb->cpu_reg.f.f_bits.c;
                    gb->cpu_reg.f.reg = 0;
                    gb->cpu_reg.f.f_bits.c = val & 1;
                    val = (val >> 1) | (carry << 7);
                    gb->cpu_reg.f.f_bits.z = (val == 0);
                    break;
                }
                case 4: /* SLA */
                    gb->cpu_reg.f.reg = 0;
                    gb->cpu_reg.f.f_bits.c = (val >> 7);
                    val = val << 1;
                    gb->cpu_reg.f.f_bits.z = (val == 0);
                    break;
                case 5: /* SRA */
                    gb->cpu_reg.f.reg = 0;
                    gb->cpu_reg.f.f_bits.c = val & 1;
                    val = (val >> 1) | (val & 0x80);
                    gb->cpu_reg.f.f_bits.z = (val == 0);
                    break;
                case 6: /* SWAP */
                    val = ((val >> 4) & 0x0F) | ((val << 4) & 0xF0);
                    gb->cpu_reg.f.reg = 0;
                    gb->cpu_reg.f.f_bits.z = (val == 0);
                    break;
                case 7: /* SRL */
                    gb->cpu_reg.f.reg = 0;
                    gb->cpu_reg.f.f_bits.c = val & 1;
                    val = val >> 1;
                    gb->cpu_reg.f.f_bits.z = (val == 0);
                    break;
            }
            break;
        }
        case 1: /* BIT */
            gb->cpu_reg.f.f_bits.z = !((val >> bit) & 1);
            gb->cpu_reg.f.f_bits.n = 0;
            gb->cpu_reg.f.f_bits.h = 1;
            writeback = false;
            cycles = (reg_idx == 6) ? 12 : 8;
            break;
        case 2: /* RES */
            val &= ~(1 << bit);
            break;
        case 3: /* SET */
            val |= (1 << bit);
            break;
    }
    
    /* Write back result */
    if (writeback) {
        switch (reg_idx) {
            case 0: gb->cpu_reg.bc.bytes.b = val; break;
            case 1: gb->cpu_reg.bc.bytes.c = val; break;
            case 2: gb->cpu_reg.de.bytes.d = val; break;
            case 3: gb->cpu_reg.de.bytes.e = val; break;
            case 4: gb->cpu_reg.hl.bytes.h = val; break;
            case 5: gb->cpu_reg.hl.bytes.l = val; break;
            case 6: mmu_write(gb, gb->cpu_reg.hl.reg, val); break;
            case 7: gb->cpu_reg.a = val; break;
        }
    }
    
    return cycles;
}



uint8_t gb_rom_read(struct gb_s* gb, const uint32_t addr){
    /* TODO */
    return gb->wram[addr];
}

void gb_write(struct gb_s* gb, uint32_t addr, const uint8_t val){
    /* TODO */
    gb->wram[addr] = val;
}


// -------------------------------
// Main CPU Step Function
// -------------------------------

uint16_t cpu_step(struct gb_s *gb) {
    uint16_t cycles;
    uint8_t opcode;
    
    /* Handle interrupts first */
    cpu_handle_interrupts(gb);
    
    /* Fetch opcode */
    opcode = mmu_read(gb, gb->cpu_reg.pc.reg++);
    cycles = OPCODE_CYCLES[opcode];
    uint16_t pc = gb->cpu_reg.pc.reg; // For debugging output

    // printf("DEBUG: PC=%04X opcode=%02X cycles=%u lcd_count=%u LY=%u\n",
    //    (unsigned)pc,
    //    opcode,
    //    cycles,
    //    gb->counter.lcd_count,
    //    gb->hram_io[IO_LY]);

    if (gb->hram_io[IO_LY] == 0 && gb->frame_debug < 10 && instr_debug < 50) {
        printf("CPU HEARTBEAT: frame=%u PC=%04X opcode=%02X LY=%u IME=%d IF=%02X IE=%02X\n",
            gb->frame_debug, pc, opcode, gb->hram_io[IO_LY], gb->gb_ime, gb->hram_io[IO_IF], gb->hram_io[IO_IE]);
        instr_debug++;
    }
    
    /* Execute opcode */
    switch (opcode) {
        /* ====== 0x0X: Misc/Control ====== */
        case 0x00: /* NOP */ break;
        case 0x10: /* STOP */ break;
        
        /* ====== 0x0X-0x3X: 16-bit loads ====== */
        case 0x01: /* LD BC, nn */
            gb->cpu_reg.bc.bytes.c = mmu_read(gb, gb->cpu_reg.pc.reg++);
            gb->cpu_reg.bc.bytes.b = mmu_read(gb, gb->cpu_reg.pc.reg++);
            break;
        case 0x11: /* LD DE, nn */
            gb->cpu_reg.de.bytes.e = mmu_read(gb, gb->cpu_reg.pc.reg++);
            gb->cpu_reg.de.bytes.d = mmu_read(gb, gb->cpu_reg.pc.reg++);
            break;
        case 0x21: /* LD HL, nn */
            gb->cpu_reg.hl.bytes.l = mmu_read(gb, gb->cpu_reg.pc.reg++);
            gb->cpu_reg.hl.bytes.h = mmu_read(gb, gb->cpu_reg.pc.reg++);
            break;
        case 0x31: /* LD SP, nn */
            gb->cpu_reg.sp.bytes.p = mmu_read(gb, gb->cpu_reg.pc.reg++);
            gb->cpu_reg.sp.bytes.s = mmu_read(gb, gb->cpu_reg.pc.reg++);
            break;
        
        /* ====== 0xX2/0xX3: 16-bit INC/DEC ====== */
        case 0x03: gb->cpu_reg.bc.reg++; break;
        case 0x13: gb->cpu_reg.de.reg++; break;
        case 0x23: gb->cpu_reg.hl.reg++; break;
        case 0x33: gb->cpu_reg.sp.reg++; break;
        case 0x0B: gb->cpu_reg.bc.reg--; break;
        case 0x1B: gb->cpu_reg.de.reg--; break;
        case 0x2B: gb->cpu_reg.hl.reg--; break;
        case 0x3B: gb->cpu_reg.sp.reg--; break;
        
        /* ====== 0xX4/0xX5: 8-bit INC/DEC ====== */
        case 0x04: CPU_INC_R8(gb->cpu_reg.bc.bytes.b); break;
        case 0x0C: CPU_INC_R8(gb->cpu_reg.bc.bytes.c); break;
        case 0x14: CPU_INC_R8(gb->cpu_reg.de.bytes.d); break;
        case 0x1C: CPU_INC_R8(gb->cpu_reg.de.bytes.e); break;
        case 0x24: CPU_INC_R8(gb->cpu_reg.hl.bytes.h); break;
        case 0x2C: CPU_INC_R8(gb->cpu_reg.hl.bytes.l); break;
        case 0x3C: CPU_INC_R8(gb->cpu_reg.a); break;
        
        case 0x05: CPU_DEC_R8(gb->cpu_reg.bc.bytes.b); break;
        case 0x0D: CPU_DEC_R8(gb->cpu_reg.bc.bytes.c); break;
        case 0x15: CPU_DEC_R8(gb->cpu_reg.de.bytes.d); break;
        case 0x1D: CPU_DEC_R8(gb->cpu_reg.de.bytes.e); break;
        case 0x25: CPU_DEC_R8(gb->cpu_reg.hl.bytes.h); break;
        case 0x2D: CPU_DEC_R8(gb->cpu_reg.hl.bytes.l); break;
        case 0x3D: CPU_DEC_R8(gb->cpu_reg.a); break;
        
        case 0x34: /* INC (HL) */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.hl.reg);
            CPU_INC_R8(val);
            mmu_write(gb, gb->cpu_reg.hl.reg, val);
            break;
        }
        case 0x35: /* DEC (HL) */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.hl.reg);
            CPU_DEC_R8(val);
            mmu_write(gb, gb->cpu_reg.hl.reg, val);
            break;
        }
        
        /* ====== 0xX6/0xXE: 8-bit immediate loads ====== */
        case 0x06: gb->cpu_reg.bc.bytes.b = mmu_read(gb, gb->cpu_reg.pc.reg++); break;
        case 0x0E: gb->cpu_reg.bc.bytes.c = mmu_read(gb, gb->cpu_reg.pc.reg++); break;
        case 0x16: gb->cpu_reg.de.bytes.d = mmu_read(gb, gb->cpu_reg.pc.reg++); break;
        case 0x1E: gb->cpu_reg.de.bytes.e = mmu_read(gb, gb->cpu_reg.pc.reg++); break;
        case 0x26: gb->cpu_reg.hl.bytes.h = mmu_read(gb, gb->cpu_reg.pc.reg++); break;
        case 0x2E: gb->cpu_reg.hl.bytes.l = mmu_read(gb, gb->cpu_reg.pc.reg++); break;
        case 0x36: mmu_write(gb, gb->cpu_reg.hl.reg, mmu_read(gb, gb->cpu_reg.pc.reg++)); break;
        case 0x3E: gb->cpu_reg.a = mmu_read(gb, gb->cpu_reg.pc.reg++); break;
        
        /* ====== 0x0X: Rotates/Misc ====== */
        case 0x07: /* RLCA */
            gb->cpu_reg.a = (gb->cpu_reg.a << 1) | (gb->cpu_reg.a >> 7);
            gb->cpu_reg.f.reg = 0;
            gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.a & 0x01);
            break;
        case 0x0F: /* RRCA */
            gb->cpu_reg.f.reg = 0;
            gb->cpu_reg.f.f_bits.c = gb->cpu_reg.a & 0x01;
            gb->cpu_reg.a = (gb->cpu_reg.a >> 1) | (gb->cpu_reg.a << 7);
            break;
        case 0x17: /* RLA */
        {
            uint8_t carry = gb->cpu_reg.f.f_bits.c;
            gb->cpu_reg.f.reg = 0;
            gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.a >> 7);
            gb->cpu_reg.a = (gb->cpu_reg.a << 1) | carry;
            break;
        }
        case 0x1F: /* RRA */
        {
            uint8_t carry = gb->cpu_reg.f.f_bits.c;
            gb->cpu_reg.f.reg = 0;
            gb->cpu_reg.f.f_bits.c = gb->cpu_reg.a & 1;
            gb->cpu_reg.a = (gb->cpu_reg.a >> 1) | (carry << 7);
            break;
        }
        
        /* ====== 0x2X/0x3X: Misc A operations ====== */
        case 0x27: /* DAA - Decimal Adjust A (BCD) */
        {
            int16_t a = gb->cpu_reg.a;
            if (gb->cpu_reg.f.f_bits.n) {
                if (gb->cpu_reg.f.f_bits.h) a = (a - 0x06) & 0xFF;
                if (gb->cpu_reg.f.f_bits.c) a -= 0x60;
            } else {
                if (gb->cpu_reg.f.f_bits.h || (a & 0x0F) > 9) a += 0x06;
                if (gb->cpu_reg.f.f_bits.c || a > 0x9F) a += 0x60;
            }
            if ((a & 0x100) == 0x100) gb->cpu_reg.f.f_bits.c = 1;
            gb->cpu_reg.a = a;
            gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0);
            gb->cpu_reg.f.f_bits.h = 0;
            break;
        }
        case 0x2F: /* CPL - Complement A */
            gb->cpu_reg.a = ~gb->cpu_reg.a;
            gb->cpu_reg.f.f_bits.n = 1;
            gb->cpu_reg.f.f_bits.h = 1;
            break;
        case 0x37: /* SCF - Set Carry Flag */
            gb->cpu_reg.f.f_bits.n = 0;
            gb->cpu_reg.f.f_bits.h = 0;
            gb->cpu_reg.f.f_bits.c = 1;
            break;
        case 0x3F: /* CCF - Complement Carry Flag */
            gb->cpu_reg.f.f_bits.n = 0;
            gb->cpu_reg.f.f_bits.h = 0;
            gb->cpu_reg.f.f_bits.c = ~gb->cpu_reg.f.f_bits.c;
            break;
        
        /* ====== 0xX8: 16-bit ADD HL ====== */
        case 0x09: /* ADD HL, BC */
        {
            uint32_t result = gb->cpu_reg.hl.reg + gb->cpu_reg.bc.reg;
            gb->cpu_reg.f.f_bits.n = 0;
            gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.hl.reg ^ gb->cpu_reg.bc.reg ^ result) & 0x1000) != 0;
            gb->cpu_reg.f.f_bits.c = (result & 0x10000) != 0;
            gb->cpu_reg.hl.reg = result & 0xFFFF;
            break;
        }
        case 0x19: /* ADD HL, DE */
        {
            uint32_t result = gb->cpu_reg.hl.reg + gb->cpu_reg.de.reg;
            gb->cpu_reg.f.f_bits.n = 0;
            gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.hl.reg ^ gb->cpu_reg.de.reg ^ result) & 0x1000) != 0;
            gb->cpu_reg.f.f_bits.c = (result & 0x10000) != 0;
            gb->cpu_reg.hl.reg = result & 0xFFFF;
            break;
        }
        // This case has been manually rewritten.
        // Before, assigning values beyond the range allowed by a 1-bit bitfield triggered an overflow 
        //   and the compiler flags this as a bug risk.
        // New approach ensures flags are set correctly and avoids the overflow error from bitfield assignment.
        case 0x29: /* ADD HL, HL */
        {
            uint16_t val = gb->cpu_reg.hl.reg;
            gb->cpu_reg.f.f_bits.n = 0;
            gb->cpu_reg.f.f_bits.h = ((val & 0x0FFF) + (val & 0x0FFF)) > 0x0FFF ? 1 : 0;
            gb->cpu_reg.f.f_bits.c = ((uint32_t)val + (uint32_t)val) > 0xFFFF ? 1 : 0;
            gb->cpu_reg.hl.reg = val + val;
            break;
        }
        // This case has been manually rewritten.
        // It had the same bug as the case above.
        case 0x39: /* ADD HL, SP */
        {
            uint32_t result = gb->cpu_reg.hl.reg + gb->cpu_reg.sp.reg;
            gb->cpu_reg.f.f_bits.n = 0;
            gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.hl.reg & 0x0FFF) + (gb->cpu_reg.sp.reg & 0x0FFF)) > 0x0FFF ? 1 : 0;
            gb->cpu_reg.f.f_bits.c = result > 0xFFFF ? 1 : 0;
            gb->cpu_reg.hl.reg = result & 0xFFFF;
            break;
        }

        /* ====== 0x08: LD (nn), SP ====== */
        case 0x08:
        {
            uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
            uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg++);
            uint16_t addr = (hi << 8) | lo;
            mmu_write(gb, addr, gb->cpu_reg.sp.bytes.p);
            mmu_write(gb, addr + 1, gb->cpu_reg.sp.bytes.s);
            break;
        }
        
        /* ====== 0xX2/0xXA: Indirect loads ====== */
        case 0x02: mmu_write(gb, gb->cpu_reg.bc.reg, gb->cpu_reg.a); break;
        case 0x12: mmu_write(gb, gb->cpu_reg.de.reg, gb->cpu_reg.a); break;
        case 0x22: mmu_write(gb, gb->cpu_reg.hl.reg++, gb->cpu_reg.a); break;
        case 0x32: mmu_write(gb, gb->cpu_reg.hl.reg--, gb->cpu_reg.a); break;
        
        case 0x0A: gb->cpu_reg.a = mmu_read(gb, gb->cpu_reg.bc.reg); break;
        case 0x1A: gb->cpu_reg.a = mmu_read(gb, gb->cpu_reg.de.reg); break;
        case 0x2A: gb->cpu_reg.a = mmu_read(gb, gb->cpu_reg.hl.reg++); break;
        case 0x3A: gb->cpu_reg.a = mmu_read(gb, gb->cpu_reg.hl.reg--); break;
        
        /* ====== 0x18/0x20/0x28/0x30/0x38: Jumps (relative) ====== */
        case 0x18: /* JR n */
        {
            int8_t offset = (int8_t)mmu_read(gb, gb->cpu_reg.pc.reg++);
            gb->cpu_reg.pc.reg += offset;
            break;
        }
        case 0x20: /* JR NZ, n */
            if (!gb->cpu_reg.f.f_bits.z) {
                int8_t offset = (int8_t)mmu_read(gb, gb->cpu_reg.pc.reg++);
                gb->cpu_reg.pc.reg += offset;
                cycles += 4;
            } else {
                gb->cpu_reg.pc.reg++;
            }
            break;
        case 0x28: /* JR Z, n */
            if (gb->cpu_reg.f.f_bits.z) {
                int8_t offset = (int8_t)mmu_read(gb, gb->cpu_reg.pc.reg++);
                gb->cpu_reg.pc.reg += offset;
                cycles += 4;
            } else {
                gb->cpu_reg.pc.reg++;
            }
            break;
        case 0x30: /* JR NC, n */
            if (!gb->cpu_reg.f.f_bits.c) {
                int8_t offset = (int8_t)mmu_read(gb, gb->cpu_reg.pc.reg++);
                gb->cpu_reg.pc.reg += offset;
                cycles += 4;
            } else {
                gb->cpu_reg.pc.reg++;
            }
            break;
        case 0x38: /* JR C, n */
            if (gb->cpu_reg.f.f_bits.c) {
                int8_t offset = (int8_t)mmu_read(gb, gb->cpu_reg.pc.reg++);
                gb->cpu_reg.pc.reg += offset;
                cycles += 4;
            } else {
                gb->cpu_reg.pc.reg++;
            }
            break;
        
        /* ====== 0x4X-0x7X: 8-bit register loads (LD r, r) ====== */
        /* B register destinations */
        case 0x40: break; /* LD B, B */
        case 0x41: gb->cpu_reg.bc.bytes.b = gb->cpu_reg.bc.bytes.c; break;
        case 0x42: gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.d; break;
        case 0x43: gb->cpu_reg.bc.bytes.b = gb->cpu_reg.de.bytes.e; break;
        case 0x44: gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.h; break;
        case 0x45: gb->cpu_reg.bc.bytes.b = gb->cpu_reg.hl.bytes.l; break;
        case 0x46: gb->cpu_reg.bc.bytes.b = mmu_read(gb, gb->cpu_reg.hl.reg); break;
        case 0x47: gb->cpu_reg.bc.bytes.b = gb->cpu_reg.a; break;
        
        /* C register destinations */
        case 0x48: gb->cpu_reg.bc.bytes.c = gb->cpu_reg.bc.bytes.b; break;
        case 0x49: break; /* LD C, C */
        case 0x4A: gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.d; break;
        case 0x4B: gb->cpu_reg.bc.bytes.c = gb->cpu_reg.de.bytes.e; break;
        case 0x4C: gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.h; break;
        case 0x4D: gb->cpu_reg.bc.bytes.c = gb->cpu_reg.hl.bytes.l; break;
        case 0x4E: gb->cpu_reg.bc.bytes.c = mmu_read(gb, gb->cpu_reg.hl.reg); break;
        case 0x4F: gb->cpu_reg.bc.bytes.c = gb->cpu_reg.a; break;
        
        /* D register destinations */
        case 0x50: gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.b; break;
        case 0x51: gb->cpu_reg.de.bytes.d = gb->cpu_reg.bc.bytes.c; break;
        case 0x52: break; /* LD D, D */
        case 0x53: gb->cpu_reg.de.bytes.d = gb->cpu_reg.de.bytes.e; break;
        case 0x54: gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.h; break;
        case 0x55: gb->cpu_reg.de.bytes.d = gb->cpu_reg.hl.bytes.l; break;
        case 0x56: gb->cpu_reg.de.bytes.d = mmu_read(gb, gb->cpu_reg.hl.reg); break;
        case 0x57: gb->cpu_reg.de.bytes.d = gb->cpu_reg.a; break;
        
        /* E register destinations */
        case 0x58: gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.b; break;
        case 0x59: gb->cpu_reg.de.bytes.e = gb->cpu_reg.bc.bytes.c; break;
        case 0x5A: gb->cpu_reg.de.bytes.e = gb->cpu_reg.de.bytes.d; break;
        case 0x5B: break; /* LD E, E */
        case 0x5C: gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.h; break;
        case 0x5D: gb->cpu_reg.de.bytes.e = gb->cpu_reg.hl.bytes.l; break;
        case 0x5E: gb->cpu_reg.de.bytes.e = mmu_read(gb, gb->cpu_reg.hl.reg); break;
        case 0x5F: gb->cpu_reg.de.bytes.e = gb->cpu_reg.a; break;
        
        /* H register destinations */
        case 0x60: gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.b; break;
        case 0x61: gb->cpu_reg.hl.bytes.h = gb->cpu_reg.bc.bytes.c; break;
        case 0x62: gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.d; break;
        case 0x63: gb->cpu_reg.hl.bytes.h = gb->cpu_reg.de.bytes.e; break;
        case 0x64: break; /* LD H, H */
        case 0x65: gb->cpu_reg.hl.bytes.h = gb->cpu_reg.hl.bytes.l; break;
        case 0x66: gb->cpu_reg.hl.bytes.h = mmu_read(gb, gb->cpu_reg.hl.reg); break;
        case 0x67: gb->cpu_reg.hl.bytes.h = gb->cpu_reg.a; break;
        
        /* L register destinations */
        case 0x68: gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.b; break;
        case 0x69: gb->cpu_reg.hl.bytes.l = gb->cpu_reg.bc.bytes.c; break;
        case 0x6A: gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.d; break;
        case 0x6B: gb->cpu_reg.hl.bytes.l = gb->cpu_reg.de.bytes.e; break;
        case 0x6C: gb->cpu_reg.hl.bytes.l = gb->cpu_reg.hl.bytes.h; break;
        case 0x6D: break; /* LD L, L */
        case 0x6E: gb->cpu_reg.hl.bytes.l = mmu_read(gb, gb->cpu_reg.hl.reg); break;
        case 0x6F: gb->cpu_reg.hl.bytes.l = gb->cpu_reg.a; break;
        
        /* (HL) destinations */
        case 0x70: mmu_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.b); break;
        case 0x71: mmu_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.bc.bytes.c); break;
        case 0x72: mmu_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.d); break;
        case 0x73: mmu_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.de.bytes.e); break;
        case 0x74: mmu_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.h); break;
        case 0x75: mmu_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.hl.bytes.l); break;
        case 0x76: /* HALT */ gb->gb_halt = true; break;
        case 0x77: mmu_write(gb, gb->cpu_reg.hl.reg, gb->cpu_reg.a); break;
        
        /* A register destinations */
        case 0x78: gb->cpu_reg.a = gb->cpu_reg.bc.bytes.b; break;
        case 0x79: gb->cpu_reg.a = gb->cpu_reg.bc.bytes.c; break;
        case 0x7A: gb->cpu_reg.a = gb->cpu_reg.de.bytes.d; break;
        case 0x7B: gb->cpu_reg.a = gb->cpu_reg.de.bytes.e; break;
        case 0x7C: gb->cpu_reg.a = gb->cpu_reg.hl.bytes.h; break;
        case 0x7D: gb->cpu_reg.a = gb->cpu_reg.hl.bytes.l; break;
        case 0x7E: gb->cpu_reg.a = mmu_read(gb, gb->cpu_reg.hl.reg); break;
        case 0x7F: break; /* LD A, A */
        
        /* ====== 0x8X: ADD/ADC operations ====== */
        case 0x80: CPU_ADC_R8(gb->cpu_reg.bc.bytes.b, 0); break;
        case 0x81: CPU_ADC_R8(gb->cpu_reg.bc.bytes.c, 0); break;
        case 0x82: CPU_ADC_R8(gb->cpu_reg.de.bytes.d, 0); break;
        case 0x83: CPU_ADC_R8(gb->cpu_reg.de.bytes.e, 0); break;
        case 0x84: CPU_ADC_R8(gb->cpu_reg.hl.bytes.h, 0); break;
        case 0x85: CPU_ADC_R8(gb->cpu_reg.hl.bytes.l, 0); break;
        case 0x86: CPU_ADC_R8(mmu_read(gb, gb->cpu_reg.hl.reg), 0); break;
        case 0x87: CPU_ADC_R8(gb->cpu_reg.a, 0); break;
        
        case 0x88: CPU_ADC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c); break;
        case 0x89: CPU_ADC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c); break;
        case 0x8A: CPU_ADC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c); break;
        case 0x8B: CPU_ADC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c); break;
        case 0x8C: CPU_ADC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c); break;
        case 0x8D: CPU_ADC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c); break;
        case 0x8E: CPU_ADC_R8(mmu_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c); break;
        case 0x8F: CPU_ADC_R8(gb->cpu_reg.a, gb->cpu_reg.f.f_bits.c); break;
        
        /* ====== 0x9X: SUB/SBC operations ====== */
        case 0x90: CPU_SBC_R8(gb->cpu_reg.bc.bytes.b, 0); break;
        case 0x91: CPU_SBC_R8(gb->cpu_reg.bc.bytes.c, 0); break;
        case 0x92: CPU_SBC_R8(gb->cpu_reg.de.bytes.d, 0); break;
        case 0x93: CPU_SBC_R8(gb->cpu_reg.de.bytes.e, 0); break;
        case 0x94: CPU_SBC_R8(gb->cpu_reg.hl.bytes.h, 0); break;
        case 0x95: CPU_SBC_R8(gb->cpu_reg.hl.bytes.l, 0); break;
        case 0x96: CPU_SBC_R8(mmu_read(gb, gb->cpu_reg.hl.reg), 0); break;
        case 0x97: /* SUB A, A */
            gb->cpu_reg.a = 0;
            gb->cpu_reg.f.reg = 0;
            gb->cpu_reg.f.f_bits.z = 1;
            gb->cpu_reg.f.f_bits.n = 1;
            break;
        
        case 0x98: CPU_SBC_R8(gb->cpu_reg.bc.bytes.b, gb->cpu_reg.f.f_bits.c); break;
        case 0x99: CPU_SBC_R8(gb->cpu_reg.bc.bytes.c, gb->cpu_reg.f.f_bits.c); break;
        case 0x9A: CPU_SBC_R8(gb->cpu_reg.de.bytes.d, gb->cpu_reg.f.f_bits.c); break;
        case 0x9B: CPU_SBC_R8(gb->cpu_reg.de.bytes.e, gb->cpu_reg.f.f_bits.c); break;
        case 0x9C: CPU_SBC_R8(gb->cpu_reg.hl.bytes.h, gb->cpu_reg.f.f_bits.c); break;
        case 0x9D: CPU_SBC_R8(gb->cpu_reg.hl.bytes.l, gb->cpu_reg.f.f_bits.c); break;
        case 0x9E: CPU_SBC_R8(mmu_read(gb, gb->cpu_reg.hl.reg), gb->cpu_reg.f.f_bits.c); break;
        case 0x9F: /* SBC A, A */
            gb->cpu_reg.a = gb->cpu_reg.f.f_bits.c ? 0xFF : 0x00;
            gb->cpu_reg.f.f_bits.z = !gb->cpu_reg.f.f_bits.c;
            gb->cpu_reg.f.f_bits.n = 1;
            gb->cpu_reg.f.f_bits.h = gb->cpu_reg.f.f_bits.c;
            break;
        
        /* ====== 0xAX: AND operations ====== */
        case 0xA0: CPU_AND_R8(gb->cpu_reg.bc.bytes.b); break;
        case 0xA1: CPU_AND_R8(gb->cpu_reg.bc.bytes.c); break;
        case 0xA2: CPU_AND_R8(gb->cpu_reg.de.bytes.d); break;
        case 0xA3: CPU_AND_R8(gb->cpu_reg.de.bytes.e); break;
        case 0xA4: CPU_AND_R8(gb->cpu_reg.hl.bytes.h); break;
        case 0xA5: CPU_AND_R8(gb->cpu_reg.hl.bytes.l); break;
        case 0xA6: CPU_AND_R8(mmu_read(gb, gb->cpu_reg.hl.reg)); break;
        case 0xA7: CPU_AND_R8(gb->cpu_reg.a); break;
        
        case 0xA8: CPU_XOR_R8(gb->cpu_reg.bc.bytes.b); break;
        case 0xA9: CPU_XOR_R8(gb->cpu_reg.bc.bytes.c); break;
        case 0xAA: CPU_XOR_R8(gb->cpu_reg.de.bytes.d); break;
        case 0xAB: CPU_XOR_R8(gb->cpu_reg.de.bytes.e); break;
        case 0xAC: CPU_XOR_R8(gb->cpu_reg.hl.bytes.h); break;
        case 0xAD: CPU_XOR_R8(gb->cpu_reg.hl.bytes.l); break;
        case 0xAE: CPU_XOR_R8(mmu_read(gb, gb->cpu_reg.hl.reg)); break;
        case 0xAF: CPU_XOR_R8(gb->cpu_reg.a); break;
        
        /* ====== 0xBX: OR/CP operations ====== */
        case 0xB0: CPU_OR_R8(gb->cpu_reg.bc.bytes.b); break;
        case 0xB1: CPU_OR_R8(gb->cpu_reg.bc.bytes.c); break;
        case 0xB2: CPU_OR_R8(gb->cpu_reg.de.bytes.d); break;
        case 0xB3: CPU_OR_R8(gb->cpu_reg.de.bytes.e); break;
        case 0xB4: CPU_OR_R8(gb->cpu_reg.hl.bytes.h); break;
        case 0xB5: CPU_OR_R8(gb->cpu_reg.hl.bytes.l); break;
        case 0xB6: CPU_OR_R8(mmu_read(gb, gb->cpu_reg.hl.reg)); break;
        case 0xB7: CPU_OR_R8(gb->cpu_reg.a); break;
        
        case 0xB8: CPU_CP_R8(gb->cpu_reg.bc.bytes.b); break;
        case 0xB9: CPU_CP_R8(gb->cpu_reg.bc.bytes.c); break;
        case 0xBA: CPU_CP_R8(gb->cpu_reg.de.bytes.d); break;
        case 0xBB: CPU_CP_R8(gb->cpu_reg.de.bytes.e); break;
        case 0xBC: CPU_CP_R8(gb->cpu_reg.hl.bytes.h); break;
        case 0xBD: CPU_CP_R8(gb->cpu_reg.hl.bytes.l); break;
        case 0xBE: CPU_CP_R8(mmu_read(gb, gb->cpu_reg.hl.reg)); break;
        case 0xBF: /* CP A, A */
            gb->cpu_reg.f.reg = 0;
            gb->cpu_reg.f.f_bits.z = 1;
            gb->cpu_reg.f.f_bits.n = 1;
            break;
        
        /* ====== 0xCX-0xFX: Control flow and misc ====== */
        case 0xC0: /* RET NZ */
            if (!gb->cpu_reg.f.f_bits.z) {
                gb->cpu_reg.pc.bytes.c = mmu_read(gb, gb->cpu_reg.sp.reg++);
                gb->cpu_reg.pc.bytes.p = mmu_read(gb, gb->cpu_reg.sp.reg++);
                cycles += 12;
            }
            break;
        case 0xC8: /* RET Z */
            if (gb->cpu_reg.f.f_bits.z) {
                gb->cpu_reg.pc.bytes.c = mmu_read(gb, gb->cpu_reg.sp.reg++);
                gb->cpu_reg.pc.bytes.p = mmu_read(gb, gb->cpu_reg.sp.reg++);
                cycles += 12;
            }
            break;
        case 0xD0: /* RET NC */
            if (!gb->cpu_reg.f.f_bits.c) {
                gb->cpu_reg.pc.bytes.c = mmu_read(gb, gb->cpu_reg.sp.reg++);
                gb->cpu_reg.pc.bytes.p = mmu_read(gb, gb->cpu_reg.sp.reg++);
                cycles += 12;
            }
            break;
        case 0xD8: /* RET C */
            if (gb->cpu_reg.f.f_bits.c) {
                gb->cpu_reg.pc.bytes.c = mmu_read(gb, gb->cpu_reg.sp.reg++);
                gb->cpu_reg.pc.bytes.p = mmu_read(gb, gb->cpu_reg.sp.reg++);
                cycles += 12;
            }
            break;
        case 0xC9: /* RET */
            gb->cpu_reg.pc.bytes.c = mmu_read(gb, gb->cpu_reg.sp.reg++);
            gb->cpu_reg.pc.bytes.p = mmu_read(gb, gb->cpu_reg.sp.reg++);
            break;
        case 0xD9: /* RETI */
            gb->cpu_reg.pc.bytes.c = mmu_read(gb, gb->cpu_reg.sp.reg++);
            gb->cpu_reg.pc.bytes.p = mmu_read(gb, gb->cpu_reg.sp.reg++);
            gb->gb_ime = true;
            break;
        
        /* POP */
        case 0xC1: /* POP BC */
            gb->cpu_reg.bc.bytes.c = mmu_read(gb, gb->cpu_reg.sp.reg++);
            gb->cpu_reg.bc.bytes.b = mmu_read(gb, gb->cpu_reg.sp.reg++);
            break;
        case 0xD1: /* POP DE */
            gb->cpu_reg.de.bytes.e = mmu_read(gb, gb->cpu_reg.sp.reg++);
            gb->cpu_reg.de.bytes.d = mmu_read(gb, gb->cpu_reg.sp.reg++);
            break;
        case 0xE1: /* POP HL */
            gb->cpu_reg.hl.bytes.l = mmu_read(gb, gb->cpu_reg.sp.reg++);
            gb->cpu_reg.hl.bytes.h = mmu_read(gb, gb->cpu_reg.sp.reg++);
            break;
        case 0xF1: /* POP AF */
        {
            uint8_t f = mmu_read(gb, gb->cpu_reg.sp.reg++);
            gb->cpu_reg.f.f_bits.z = (f >> 7) & 1;
            gb->cpu_reg.f.f_bits.n = (f >> 6) & 1;
            gb->cpu_reg.f.f_bits.h = (f >> 5) & 1;
            gb->cpu_reg.f.f_bits.c = (f >> 4) & 1;
            gb->cpu_reg.a = mmu_read(gb, gb->cpu_reg.sp.reg++);
            break;
        }
        
        /* JP conditional */
        case 0xC2: /* JP NZ, nn */
            if (!gb->cpu_reg.f.f_bits.z) {
                uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
                uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg);
                gb->cpu_reg.pc.reg = (hi << 8) | lo;
                cycles += 4;
            } else {
                gb->cpu_reg.pc.reg += 2;
            }
            break;
        case 0xCA: /* JP Z, nn */
            if (gb->cpu_reg.f.f_bits.z) {
                uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
                uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg);
                gb->cpu_reg.pc.reg = (hi << 8) | lo;
                cycles += 4;
            } else {
                gb->cpu_reg.pc.reg += 2;
            }
            break;
        case 0xD2: /* JP NC, nn */
            if (!gb->cpu_reg.f.f_bits.c) {
                uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
                uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg);
                gb->cpu_reg.pc.reg = (hi << 8) | lo;
                cycles += 4;
            } else {
                gb->cpu_reg.pc.reg += 2;
            }
            break;
        case 0xDA: /* JP C, nn */
            if (gb->cpu_reg.f.f_bits.c) {
                uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
                uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg);
                gb->cpu_reg.pc.reg = (hi << 8) | lo;
                cycles += 4;
            } else {
                gb->cpu_reg.pc.reg += 2;
            }
            break;
        case 0xC3: /* JP nn */
        {
            uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
            uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg);
            gb->cpu_reg.pc.reg = (hi << 8) | lo;
            break;
        }
        case 0xE9: /* JP (HL) */
            gb->cpu_reg.pc.reg = gb->cpu_reg.hl.reg;
            break;
        
        /* CALL conditional */
        case 0xC4: /* CALL NZ, nn */
            if (!gb->cpu_reg.f.f_bits.z) {
                uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
                uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg++);
                mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
                mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                gb->cpu_reg.pc.reg = (hi << 8) | lo;
                cycles += 12;
            } else {
                gb->cpu_reg.pc.reg += 2;
            }
            break;
        case 0xCC: /* CALL Z, nn */
            if (gb->cpu_reg.f.f_bits.z) {
                uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
                uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg++);
                mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
                mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                gb->cpu_reg.pc.reg = (hi << 8) | lo;
                cycles += 12;
            } else {
                gb->cpu_reg.pc.reg += 2;
            }
            break;
        case 0xD4: /* CALL NC, nn */
            if (!gb->cpu_reg.f.f_bits.c) {
                uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
                uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg++);
                mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
                mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                gb->cpu_reg.pc.reg = (hi << 8) | lo;
                cycles += 12;
            } else {
                gb->cpu_reg.pc.reg += 2;
            }
            break;
        case 0xDC: /* CALL C, nn */
            if (gb->cpu_reg.f.f_bits.c) {
                uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
                uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg++);
                mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
                mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                gb->cpu_reg.pc.reg = (hi << 8) | lo;
                cycles += 12;
            } else {
                gb->cpu_reg.pc.reg += 2;
            }
            break;
        case 0xCD: /* CALL nn */
        {
            uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
            uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg++);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
            gb->cpu_reg.pc.reg = (hi << 8) | lo;
            break;
        }
        
        /* PUSH */
        case 0xC5: /* PUSH BC */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.bc.bytes.b);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.bc.bytes.c);
            break;
        case 0xD5: /* PUSH DE */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.de.bytes.d);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.de.bytes.e);
            break;
        case 0xE5: /* PUSH HL */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.hl.bytes.h);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.hl.bytes.l);
            break;
        case 0xF5: /* PUSH AF */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.a);
            mmu_write(gb, --gb->cpu_reg.sp.reg,
                gb->cpu_reg.f.f_bits.z << 7 | gb->cpu_reg.f.f_bits.n << 6 |
                gb->cpu_reg.f.f_bits.h << 5 | gb->cpu_reg.f.f_bits.c << 4);
            break;
        
        /* RST (Reset/Call to fixed address) */
        case 0xC7: /* RST 00H */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
            gb->cpu_reg.pc.reg = 0x0000;
            break;
        case 0xCF: /* RST 08H */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
            gb->cpu_reg.pc.reg = 0x0008;
            break;
        case 0xD7: /* RST 10H */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
            gb->cpu_reg.pc.reg = 0x0010;
            break;
        case 0xDF: /* RST 18H */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
            gb->cpu_reg.pc.reg = 0x0018;
            break;
        case 0xE7: /* RST 20H */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
            gb->cpu_reg.pc.reg = 0x0020;
            break;
        case 0xEF: /* RST 28H */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
            gb->cpu_reg.pc.reg = 0x0028;
            break;
        case 0xF7: /* RST 30H */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
            gb->cpu_reg.pc.reg = 0x0030;
            break;
        case 0xFF: /* RST 38H */
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
            mmu_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
            gb->cpu_reg.pc.reg = 0x0038;
            break;
        
        /* Immediate ALU operations */
        case 0xC6: /* ADD A, n */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.pc.reg++);
            CPU_ADC_R8(val, 0);
            break;
        }
        case 0xCE: /* ADC A, n */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.pc.reg++);
            CPU_ADC_R8(val, gb->cpu_reg.f.f_bits.c);
            break;
        }
        case 0xD6: /* SUB n */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.pc.reg++);
            CPU_SBC_R8(val, 0);
            break;
        }
        case 0xDE: /* SBC A, n */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.pc.reg++);
            CPU_SBC_R8(val, gb->cpu_reg.f.f_bits.c);
            break;
        }
        case 0xE6: /* AND n */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.pc.reg++);
            CPU_AND_R8(val);
            break;
        }
        case 0xEE: /* XOR n */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.pc.reg++);
            CPU_XOR_R8(val);
            break;
        }
        case 0xF6: /* OR n */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.pc.reg++);
            CPU_OR_R8(val);
            break;
        }
        case 0xFE: /* CP n */
        {
            uint8_t val = mmu_read(gb, gb->cpu_reg.pc.reg++);
            CPU_CP_R8(val);
            break;
        }
        
        /* I/O operations */
        case 0xE0: /* LDH (n), A */
            mmu_write(gb, 0xFF00 | mmu_read(gb, gb->cpu_reg.pc.reg++), gb->cpu_reg.a);
            break;
        case 0xF0: /* LDH A, (n) */
            gb->cpu_reg.a = mmu_read(gb, 0xFF00 | mmu_read(gb, gb->cpu_reg.pc.reg++));
            break;
        case 0xE2: /* LD (C), A */
            mmu_write(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a);
            break;
        case 0xF2: /* LD A, (C) */
            gb->cpu_reg.a = mmu_read(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c);
            break;
        
        /* Direct memory operations */
        case 0xEA: /* LD (nn), A */
        {
            uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
            uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg++);
            uint16_t addr = (hi << 8) | lo;
            mmu_write(gb, addr, gb->cpu_reg.a);
            break;
        }
        case 0xFA: /* LD A, (nn) */
        {
            uint8_t lo = mmu_read(gb, gb->cpu_reg.pc.reg++);
            uint8_t hi = mmu_read(gb, gb->cpu_reg.pc.reg++);
            uint16_t addr = (hi << 8) | lo;
            gb->cpu_reg.a = mmu_read(gb, addr);
            break;
        }
        
        /* Stack pointer operations */
        case 0xE8: /* ADD SP, n */
        {
            int8_t offset = (int8_t)mmu_read(gb, gb->cpu_reg.pc.reg++);
            gb->cpu_reg.f.reg = 0;
            gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF)) > 0xF;
            gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF)) > 0xFF;
            gb->cpu_reg.sp.reg += offset;
            break;
        }
        case 0xF8: /* LD HL, SP+n */
        {
            int8_t offset = (int8_t)mmu_read(gb, gb->cpu_reg.pc.reg++);
            gb->cpu_reg.hl.reg = gb->cpu_reg.sp.reg + offset;
            gb->cpu_reg.f.reg = 0;
            gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (offset & 0xF)) > 0xF;
            gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (offset & 0xFF)) > 0xFF;
            break;
        }
        case 0xF9: /* LD SP, HL */
            gb->cpu_reg.sp.reg = gb->cpu_reg.hl.reg;
            break;
        
        /* Interrupt control */
        case 0xF3: /* DI */
            gb->gb_ime = false;
            break;
        case 0xFB: /* EI */
            gb->gb_ime = true;
            break;
        
        /* CB prefix */
        case 0xCB:
            cycles = cpu_execute_cb(gb);
            break;
        
        /* Invalid/unused opcodes */
        default:
            /* Call error handler */
            if (gb->gb_error) {
                gb->gb_error(gb, GB_ERROR_INVALID_OPCODE, gb->cpu_reg.pc.reg - 1);
            }
            break;
    }

    /* DIV register timing */
    gb->counter.div_count += cycles;

    while(gb->counter.div_count >= DIV_CYCLES){
        gb->hram_io[IO_DIV]++;
        gb->counter.div_count -= DIV_CYCLES;
    }

    /* LCD Timing */
    gb->counter.lcd_count += cycles;

    /* New Scanline. HBlank -> VBlank or OAM Scan */
    if(gb->counter.lcd_count >= LCD_LINE_CYCLES){

        gb->counter.lcd_count -= LCD_LINE_CYCLES;

        /* Next line */
        gb->hram_io[IO_LY]++;

        // if(gb->hram_io[IO_LY] == LCD_VERT_LINES) {
        //     gb->hram_io[IO_LY] = 0;
        //     printf("DEBUG: LY wrapped to 0 (end of frame)\n");
        // }

        // LY + mode debug for first few frames
        if (gb->frame_debug < 5 && gb->hram_io[IO_LY] == 0) {
            printf("DEBUG: Frame %u start: LY=%u mode=%u lcd_count=%u\n",
                gb->frame_debug,
                gb->hram_io[IO_LY],
                gb->hram_io[IO_STAT] & STAT_MODE,
                gb->counter.lcd_count);
        }

        /* LYC Update */
        if(gb->hram_io[IO_LY] == gb->hram_io[IO_LYC]){
            gb->hram_io[IO_STAT] |= STAT_LYC_COINC;

            if(gb->hram_io[IO_STAT] & STAT_LYC_INTR) gb->hram_io[IO_IF] |= LCDC_INTR;
        } else {
            gb->hram_io[IO_STAT] &= 0xFB;
        }

        /* Check if LCD should be in Mode 1 (VBLANK) state */
        if(gb->hram_io[IO_LY] == LCD_HEIGHT){
            gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | LCD_MODE_VBLANK;
            gb->gb_frame = true;
            gb->hram_io[IO_IF] |= VBLANK_INTR;
            gb->lcd_blank = false;

            if(gb->hram_io[IO_STAT] & STAT_MODE_1_INTR) gb->hram_io[IO_IF] |= LCDC_INTR;

            gb->frame_debug++;   // increment once per frame

        /* Start of normal Line (not in VBLANK) */
        } else if(gb->hram_io[IO_LY] < LCD_HEIGHT){ 
            if(gb->hram_io[IO_LY] == 0){
                /* Clear Screen */
                gb->display.WY = gb->hram_io[IO_WY];
                gb->display.window_clear = 0;
            }

            /* OAM Search occurs at the start of the line. */
            gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | LCD_MODE_OAM_SCAN;

            if(gb->hram_io[IO_STAT] & STAT_MODE_2_INTR) gb->hram_io[IO_IF] |= LCDC_INTR;
        }

    // Go from Mode 3 (LCD Draw) to Mode 0 (HBLANK).
    // Bugfix: Moved gpu_draw_line() callback to the correct place in the code.
    //   The gpu_draw_line() function doesn't do the actual PPU math;
    //   it assumes that the PPU has already rendered that scanline into pixels[160].
    } else if((gb->hram_io[IO_STAT] & STAT_MODE) == LCD_MODE_LCD_DRAW  && 
                gb->counter.lcd_count >= LCD_MODE3_LCD_DRAW_END){ 
        gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | LCD_MODE_HBLANK;

        if(!gb->lcd_blank) gpu_draw_line(gb);

        if(gb->hram_io[IO_STAT] & STAT_MODE_0_INTR) gb->hram_io[IO_IF] |= LCDC_INTR;

    /* Go from Mode 2 (OAM Scan) to Mode 3 (LCD Draw). */
    } else if((gb->hram_io[IO_STAT] & STAT_MODE) == LCD_MODE_OAM_SCAN &&
                gb->counter.lcd_count >= LCD_MODE2_OAM_SCAN_END){
        gb->hram_io[IO_STAT] = (gb->hram_io[IO_STAT] & ~STAT_MODE) | LCD_MODE_LCD_DRAW;
        // Remove gpu_draw_line() from here
    }


    return cycles;
}

// -------------------------------
// CPU Initialization and Reset
// -------------------------------

void cpu_init(struct gb_s* gb) {
    // Initialize to post-boot state (as if boot ROM already ran)
    // TODO: Fix this after Dan's bootloader code is integrated
    gb->cpu_reg.a = 0x01;
    gb->cpu_reg.f.reg = 0xB0;  /* Z=1, N=0, H=1, C=1 */
    gb->cpu_reg.bc.reg = 0x0013;
    gb->cpu_reg.de.reg = 0x00D8;
    gb->cpu_reg.hl.reg = 0x014D;
    gb->cpu_reg.sp.reg = 0xFFFE;
    gb->cpu_reg.pc.reg = 0x0100;
    
    gb->gb_halt = false;
    gb->gb_ime = true;
}

void cpu_reset(struct gb_s* gb) {
    // Reset to power-on state
    gb->cpu_reg.pc.reg = 0x0000;
    gb->gb_halt = false;
    gb->gb_ime = false;
}
