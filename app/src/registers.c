#include "registers.h"
#include "gb_types.h"
#include <stdio.h>
#include <stdlib.h>

// placeholder initializer
void regInit(){}
void regShutdown(){}

void regSet8(struct gb_s* gb, const uint8_t reg, const uint8_t val){
    switch(reg){
        case 0x07: gb->cpu_reg.a = val; break;
        case 0x00: gb->cpu_reg.bc.bytes.b = val; break;
        case 0x01: gb->cpu_reg.bc.bytes.c = val; break;
        case 0x02: gb->cpu_reg.de.bytes.d = val; break;
        case 0x03: gb->cpu_reg.de.bytes.e = val; break;
        case 0x04: gb->cpu_reg.hl.bytes.h = val; break;
        case 0x05: gb->cpu_reg.hl.bytes.l = val; break;
        default: break;
    }
}

void regSet16(struct gb_s* gb, const uint8_t reg, const uint16_t val){
    switch(reg){
        case 0x0: gb->cpu_reg.bc.reg = val; break;
        case 0x1: gb->cpu_reg.de.reg = val; break;
        case 0x2: gb->cpu_reg.hl.reg = val; break;
        case 0x3: gb->cpu_reg.sp.reg = val; break;
        default: break;
    }    
}

uint16_t regSet16stk(struct gb_s* gb, const uint8_t reg, const uint16_t val){
    switch(reg){
        case 0x0: gb->cpu_reg.bc.reg = val; break;
        case 0x1: gb->cpu_reg.de.reg = val; break;
        case 0x2: gb->cpu_reg.hl.reg = val; break;
        case 0x3: 
            gb->cpu_reg.a = val >> 8;
            gb->cpu_reg.f.reg = val & 0x7;
        default: break;
    }
    return 0x0000;
}

uint8_t regGet8(struct gb_s* gb, const uint8_t reg){
    switch(reg){
        case 0x07: return gb->cpu_reg.a;
        case 0x00: return gb->cpu_reg.bc.bytes.b;
        case 0x01: return gb->cpu_reg.bc.bytes.c;
        case 0x02: return gb->cpu_reg.de.bytes.d;
        case 0x03: return gb->cpu_reg.de.bytes.e;
        case 0x04: return gb->cpu_reg.hl.bytes.h;
        case 0x05: return gb->cpu_reg.hl.bytes.l;
        default: break;
    }
    return 0x00;
}

uint16_t regGet16(struct gb_s* gb, const uint8_t reg){
    switch(reg){
        case 0x0: return gb->cpu_reg.bc.reg;
        case 0x1: return gb->cpu_reg.de.reg;
        case 0x2: return gb->cpu_reg.hl.reg;
        case 0x3: return gb->cpu_reg.sp.reg;
        default: break;
    }
    return 0x0000;
}

uint16_t regGet16stk(struct gb_s* gb, const uint8_t reg){
    switch(reg){
        case 0x0: return gb->cpu_reg.bc.reg;
        case 0x1: return gb->cpu_reg.de.reg;
        case 0x2: return gb->cpu_reg.hl.reg;
        case 0x3: return ((uint16_t) gb->cpu_reg.a >> 8) | gb->cpu_reg.f.reg;
        default: break;
    }
    return 0x0000;
}

uint16_t regGet16mem(struct gb_s* gb, const uint8_t reg){
    switch(reg){
        case 0x0: return gb->cpu_reg.bc.reg;
        case 0x1: return gb->cpu_reg.de.reg;
        case 0x2: return gb->cpu_reg.hl.reg++;
        case 0x3: return gb->cpu_reg.sp.reg--;
        default: break;
    }
    return 0x0000;
}