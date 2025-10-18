#include "registers.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// 8-bit registers, pairs are AF, BC, DE, HL
static uint8_t A = 0x00;
static uint8_t B = 0x00;
static uint8_t C = 0x00;
static uint8_t D = 0x00;
static uint8_t E = 0x00;
static uint8_t F = 0x00;
static uint8_t H = 0x00;
static uint8_t L = 0x00;

// 16-bit registers
static uint16_t SP = 0x0000;
static uint16_t PC = 0x0000;

// placeholder initializer
void regInit(){
    PC = 0x0000;
    F = 0x00;
}

void regShutdown(){}

void setReg8(uint8_t reg, uint8_t i){
    switch(reg){
        case 0x07: A = i; break;
        case 0x00: B = i; break;
        case 0x01: C = i; break;
        case 0x02: D = i; break;
        case 0x03: E = i; break;
        case 0x04: H = i; break;
        case 0x05: L = i; break;
        default: break;
    }
}

void setReg16(uint8_t reg, uint16_t i){
    switch(reg){
        case 0x0: 
            C = i;
            B = i >> 8; 
            break;
        case 0x1: 
            E = i;
            D = i >> 8; 
            break;
        case 0x2: 
            L = i;
            H = i >> 8; 
            break;
        case 0x3: SP = i; break;
        default: break;
    }    
}

void copyReg(uint8_t reg1, uint8_t reg2){
    setReg8(reg1, getReg8(reg2));
}

uint8_t getReg8(uint8_t reg){
    switch(reg){
        case 0x07: return A;
        case 0x00: return B;
        case 0x01: return C;
        case 0x02: return D;
        case 0x03: return E;
        case 0x04: return H;
        case 0x05: return L;
        default: break;
    }
    return 0x00;
}

uint16_t getReg16(uint8_t reg){
    switch(reg){
        case 0x0: return ((uint16_t)B << 8) + C;
        case 0x1: return ((uint16_t)D << 8) + E;
        case 0x2: return ((uint16_t)H << 8) + L;
        case 0x3: return SP;
        default: break;
    }
    return 0x0000;
}