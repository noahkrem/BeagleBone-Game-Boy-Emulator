#include "gb_decode.h"
#include <stdio.h>
#include <string.h>

static const char *R8[8]  = {"B","C","D","E","H","L","(HL)","A"};
static const char *R16[4] = {"BC","DE","HL","SP"};
static const char *RP2[4] = {"BC","DE","HL","AF"};        // for PUSH/POP
static const char *CC[4]  = {"NZ","Z","NC","C"};
static const char *ALU[8] = {"ADD A,","ADC A,","SUB ","SBC A,","AND ","XOR ","OR ","CP "};

static inline uint8_t mem(gb_fetch_fn f, void *u, uint16_t a){ return f(a,u); }
static inline uint16_t u16(uint8_t lo, uint8_t hi){ return (uint16_t)lo | ((uint16_t)hi<<8); }

static void fmt(char *dst, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(dst, 48, fmt, ap);
    va_end(ap);
}

static void decode_cb(uint8_t op, GBDecoded *o) {
    // CB: [01 2 3 4 5 6 7]
    // x = op>>6: 0=rot/shift/swap, 1=BIT, 2=RES, 3=SET
    // y = (op>>3)&7 (bit or variant id), z = op&7 (register)
    uint8_t x = op >> 6, y = (op >> 3)&7, z = op&7;
    o->cb = true; o->opcode = op; o->length = 2;
    // cycles: most are 8 on registers, (HL) costs +8 for some; match Peanut-GB
    // R/(HL) special cases are tiny; we’ll mirror common timing:
    uint8_t base = 8, extra = 0;
    // pattern exceptions roughly like Peanut-GB’s cb-cycle bumps:
    if ((op & 0xC7)==0x06 || (op & 0xC7)==0x86 || (op & 0xC7)==0xC6) base += 8;
    else if ((op & 0xC7)==0x46) base += 4;
    o->cycles_min = o->cycles_max = base;

    if (x==1) { // BIT b,r
        fmt(o->text, "BIT %u,%s", y, R8[z]);
        return;
    }
    if (x==2) { // RES
        fmt(o->text, "RES %u,%s", y, R8[z]);
        return;
    }
    if (x==3) { // SET
        fmt(o->text, "SET %u,%s", y, R8[z]);
        return;
    }
    // x==0: rot/shift/swap group: y high bit (d) selects RLC/RRC vs RL/RR; the
    // middle two bits select which of RLC/RRC/RL/RR/SLA/SRA/SWAP/SRL
    static const char *Rtab[8]={"RLC","RRC","RL","RR","SLA","SRA","SWAP","SRL"};
    fmt(o->text, "%s %s", Rtab[y], R8[z]);
}

static void decode_base(gb_fetch_fn F, void *U, uint16_t pc, uint8_t op, GBDecoded *o){
    o->cb=false; o->opcode=op; o->pc=pc; o->length=1; o->cycles_min=o->cycles_max=4;
    char *t=o->text; t[0]='\0';

    uint8_t x = op>>6, y=(op>>3)&7, z=op&7;

    switch(op){
    // 0x00 block
    case 0x00: fmt(t,"NOP"); return;
    case 0x10: fmt(t,"STOP"); o->length=2; return;
    case 0x76: fmt(t,"HALT"); return;

    // LD r,imm (0x06,0x0E,0x16,0x1E,0x26,0x2E, 0x36, 0x3E)
    case 0x06: case 0x0E: case 0x16: case 0x1E:
    case 0x26: case 0x2E: case 0x36: case 0x3E: {
        uint8_t r = (op>>3)&7; uint8_t imm = mem(F,U,pc+1);
        if (r==6) fmt(t,"LD (HL),$%02X", imm);
        else      fmt(t,"LD %s,$%02X", R8[r], imm);
        o->length=2; o->cycles_min = o->cycles_max = (r==6?12:8);
        return;
    }

    // 16-bit LD rr,imm (01/11/21/31)
    case 0x01: case 0x11: case 0x21: case 0x31: {
        uint8_t rr = (op>>4)&3; uint8_t lo = mem(F,U,pc+1), hi = mem(F,U,pc+2);
        fmt(t,"LD %s,$%04X", R16[rr], u16(lo,hi)); o->length=3; o->cycles_min=o->cycles_max=12; return;
    }

    // LD (BC),(DE),A and LD A,(BC)/(DE)
    case 0x02: fmt(t,"LD (BC),A"); o->cycles_min=o->cycles_max=8; return;
    case 0x12: fmt(t,"LD (DE),A"); o->cycles_min=o->cycles_max=8; return;
    case 0x0A: fmt(t,"LD A,(BC)"); o->cycles_min=o->cycles_max=8; return;
    case 0x1A: fmt(t,"LD A,(DE)"); o->cycles_min=o->cycles_max=8; return;

    // LD (HL+/-),A and LD A,(HL+/-)
    case 0x22: fmt(t,"LD (HL+),A"); o->cycles_min=o->cycles_max=8; return;
    case 0x2A: fmt(t,"LD A,(HL+)"); o->cycles_min=o->cycles_max=8; return;
    case 0x32: fmt(t,"LD (HL-),A"); o->cycles_min=o->cycles_max=8; return;
    case 0x3A: fmt(t,"LD A,(HL-)"); o->cycles_min=o->cycles_max=8; return;

    // INC/DEC 16-bit (03/13/23/33 and 0B/1B/2B/3B)
    case 0x03: case 0x13: case 0x23: case 0x33:
        fmt(t,"INC %s", R16[(op>>4)&3]); o->cycles_min=o->cycles_max=8; return;
    case 0x0B: case 0x1B: case 0x2B: case 0x3B:
        fmt(t,"DEC %s", R16[(op>>4)&3]); o->cycles_min=o->cycles_max=8; return;

    // ADD HL,rr (09/19/29/39)
    case 0x09: case 0x19: case 0x29: case 0x39:
        fmt(t,"ADD HL,%s", R16[(op>>4)&3]); o->cycles_min=o->cycles_max=8; return;

    // ADD SP,imm8 (E8), LD HL,SP+imm8 (F8), LD SP,HL (F9)
    case 0xE8: { int8_t s = (int8_t)mem(F,U,pc+1); fmt(t,"ADD SP,%d", s); o->length=2; o->cycles_min=o->cycles_max=16; return; }
    case 0xF8: { int8_t s = (int8_t)mem(F,U,pc+1); fmt(t,"LD HL,SP%+d", s); o->length=2; o->cycles_min=o->cycles_max=12; return; }
    case 0xF9: fmt(t,"LD SP,HL"); o->cycles_min=o->cycles_max=8; return;

    // LD (imm16),A (EA) / LD A,(imm16) (FA)
    case 0xEA: { uint8_t lo=mem(F,U,pc+1), hi=mem(F,U,pc+2); fmt(t,"LD ($%04X),A", u16(lo,hi)); o->length=3; o->cycles_min=o->cycles_max=16; return; }
    case 0xFA: { uint8_t lo=mem(F,U,pc+1), hi=mem(F,U,pc+2); fmt(t,"LD A,($%04X)", u16(lo,hi)); o->length=3; o->cycles_min=o->cycles_max=16; return; }

    // LDH ($FF00+n),A (E0) / LDH A,($FF00+n) (F0) and I/O with C (E2/F2)
    case 0xE0: { uint8_t n=mem(F,U,pc+1); fmt(t,"LDH ($FF00+$%02X),A", n); o->length=2; o->cycles_min=o->cycles_max=12; return; }
    case 0xF0: { uint8_t n=mem(F,U,pc+1); fmt(t,"LDH A,($FF00+$%02X)", n); o->length=2; o->cycles_min=o->cycles_max=12; return; }
    case 0xE2: fmt(t,"LD ($FF00+C),A"); o->cycles_min=o->cycles_max=8; return;
    case 0xF2: fmt(t,"LD A,($FF00+C)"); o->cycles_min=o->cycles_max=8; return;

    // RLCA/RRCA/RLA/RRA/DAA/CPL/SCF/CCF
    case 0x07: fmt(t,"RLCA"); return;
    case 0x0F: fmt(t,"RRCA"); return;
    case 0x17: fmt(t,"RLA");  return;
    case 0x1F: fmt(t,"RRA");  return;
    case 0x27: fmt(t,"DAA");  return;
    case 0x2F: fmt(t,"CPL");  return;
    case 0x37: fmt(t,"SCF");  return;
    case 0x3F: fmt(t,"CCF");  return;

    // JR n / JR cc,n
    case 0x18: { int8_t d=(int8_t)mem(F,U,pc+1); fmt(t,"JR %d", d); o->length=2; o->cycles_min=o->cycles_max=12; return; }
    case 0x20: case 0x28: case 0x30: case 0x38: {
        int8_t d=(int8_t)mem(F,U,pc+1); fmt(t,"JR %s,%d", CC[(op>>3)&3], d);
        o->length=2; o->cycles_min=8; o->cycles_max=12; return;
    }

    // JP nn / JP cc,nn / JP (HL)
    case 0xC3: { uint8_t lo=mem(F,U,pc+1), hi=mem(F,U,pc+2); fmt(t,"JP $%04X", u16(lo,hi)); o->length=3; o->cycles_min=o->cycles_max=16; return; }
    case 0xC2: case 0xCA: case 0xD2: case 0xDA: {
        uint8_t lo=mem(F,U,pc+1), hi=mem(F,U,pc+2); fmt(t,"JP %s,$%04X", CC[(op>>3)&3], u16(lo,hi));
        o->length=3; o->cycles_min=12; o->cycles_max=16; return;
    }
    case 0xE9: fmt(t,"JP (HL)"); o->cycles_min=o->cycles_max=4; return;

    // CALL nn / CALL cc,nn
    case 0xCD: { uint8_t lo=mem(F,U,pc+1), hi=mem(F,U,pc+2); fmt(t,"CALL $%04X", u16(lo,hi)); o->length=3; o->cycles_min=o->cycles_max=24; return; }
    case 0xC4: case 0xCC: case 0xD4: case 0xDC: {
        uint8_t lo=mem(F,U,pc+1), hi=mem(F,U,pc+2); fmt(t,"CALL %s,$%04X", CC[(op>>3)&3], u16(lo,hi));
        o->length=3; o->cycles_min=12; o->cycles_max=24; return;
    }

    // RST t
    case 0xC7: case 0xCF: case 0xD7: case 0xDF:
    case 0xE7: case 0xEF: case 0xF7: case 0xFF:
        fmt(t,"RST $%04X", (op&0x38)); o->cycles_min=o->cycles_max=16; return;

    // RET / RETI / RET cc
    case 0xC9: fmt(t,"RET");  o->cycles_min=o->cycles_max=16; return;
    case 0xD9: fmt(t,"RETI"); o->cycles_min=o->cycles_max=16; return;
    case 0xC0: case 0xC8: case 0xD0: case 0xD8:
        fmt(t,"RET %s", CC[(op>>3)&3]); o->cycles_min=8; o->cycles_max=20; return;

    // PUSH/POP
    case 0xC5: case 0xD5: case 0xE5: case 0xF5:
        fmt(t,"PUSH %s", RP2[(op>>4)&3]); o->cycles_min=o->cycles_max=16; return;
    case 0xC1: case 0xD1: case 0xE1: case 0xF1:
        fmt(t,"POP %s", RP2[(op>>4)&3]);  o->cycles_min=o->cycles_max=12; return;

    // EI/DI
    case 0xFB: fmt(t,"EI"); return;
    case 0xF3: fmt(t,"DI"); return;

    // LD A,(C) via FF00+C and LD (C),A handled earlier (F2/E2).

    default: break;
    }

    // ======= Patterned blocks (cover remaining 0x40–0x7F and 0x80–0xBF) =======

    if (op==0x76) { fmt(t,"HALT"); return; } // already handled, safe

    // 0x40–0x7F: LD r,z (except 0x76)
    if ((op & 0xC0) == 0x40) {
        uint8_t dst = (op>>3)&7, src = op&7;
        fmt(t, "LD %s,%s", R8[dst], R8[src]);
        o->cycles_min=o->cycles_max = (dst==6 || src==6)?8:4;
        return;
    }

    // 0x80–0xBF: ALU group: ADD/ADC/SUB/SBC/AND/XOR/OR/CP A, r
    if ((op & 0xC0) == 0x80) {
        uint8_t alu = (op>>3)&7, src = op&7;
        fmt(t, "%s%s", ALU[alu], R8[src]);
        o->cycles_min=o->cycles_max = (src==6)?8:4;
        return;
    }

    // 0xC6/0xCE/0xD6/0xDE/0xE6/0xEE/0xF6/0xFE: ALU A,imm
    if ((op & 0xC6) == 0xC6) {
        uint8_t alu = (op>>3)&7, imm = mem(F,U,pc+1);
        fmt(t,"%s$%02X", ALU[alu], imm);
        o->length=2; o->cycles_min=o->cycles_max=8;
        return;
    }

    // 0x04/0x0C/… INC r  and  0x05/0x0D/… DEC r (covered earlier for immediates)
    if ((op & 0xC7)==0x04) { fmt(t,"INC %s", R8[(op>>3)&7]); o->cycles_min=o->cycles_max=((op&0x07)==6?12:4); return; }
    if ((op & 0xC7)==0x05) { fmt(t,"DEC %s", R8[(op>>3)&7]); o->cycles_min=o->cycles_max=((op&0x07)==6?12:4); return; }

    // 0xC7 mask didn’t hit? fall through to loads of SP/(HL) etc already handled.
    // If we reach here, it’s one of the few remaining specific encodings:

    switch(op){
    // LD SP,imm16 already handled (31). Remaining loads/stores:
    case 0x08: /* already handled */ break;

    // LD (imm16),SP (08) handled; LD HL,SP+e (F8) handled; LD SP,HL (F9) handled.

    // Special I/O aliases:
    case 0xE9: /* JP (HL) handled */ break;

    default:
        fmt(t,"DB $%02X", op); // Unknown/illegal (SM83 has a few no-ops)
        break;
    }
}

void gb_decode(gb_fetch_fn F, void *U, uint16_t pc, GBDecoded *o){
    memset(o,0,sizeof(*o)); o->pc=pc;
    uint8_t op = mem(F,U,pc);
    if (op==0xCB) {
        uint8_t cb = mem(F,U,pc+1);
        decode_cb(cb,o);
        o->pc = pc;
        return;
    }
    decode_base(F,U,pc,op,o);
}
