#include "registers.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// 8-bit registers, pairs are AF, BC, DE, HL
static uint8_t A = 0b00000000;
static uint8_t B = 0b00000000;
static uint8_t C = 0b00000000;
static uint8_t D = 0b00000000;
static uint8_t E = 0b00000000;
static uint8_t F = 0b00000000;
static uint8_t H = 0b00000000;
static uint8_t L = 0b00000000;

// 16-bit registers
static uint16_t SP = 0x0000;
static uint16_t PC = 0x0000;

void regInit(){}
void regShutdown(){}

void setReg8(uint8_t reg, uint8_t i){
    switch(reg){
        case 0b0111: A = i; break;
        case 0b0000: B = i; break;
        case 0b0001: C = i; break;
        case 0b0010: D = i; break;
        case 0b0011: E = i; break;
        case 0b0100: H = i; break;
        case 0x0101: L = i; break;
        default: break;
    }
}

void setReg16(uint8_t reg, uint16_t i){
    switch(reg){
        case 0b00: 
            C = i;
            B = i >> 8; 
            break;
        case 0b01: 
            E = i;
            D = i >> 8; 
            break;
        case 0b10: 
            L = i;
            H = i >> 8; 
            break;
        case 0b11: SP = i; break;
        default: break;
    }    
}

void copyReg(uint8_t reg1, uint8_t reg2){
    setReg8(reg1, getReg8(reg2));
}

uint8_t getReg8(uint8_t reg){
    switch(reg){
        case 0b0111: return A;
        case 0b0000: return B;
        case 0b0001: return C;
        case 0b0010: return D;
        case 0b0011: return E;
        case 0b0100: return H;
        case 0x0101: return L;
        default: break;
    }
    return 0x00;
}

uint16_t getReg16(uint8_t reg){
    switch(reg){
        case 0b00: return ((uint16_t)B << 8) + C;
        case 0b01: return ((uint16_t)D << 8) + E;
        case 0b10: return ((uint16_t)H << 8) + L;
        case 0b11: return SP;
        default: break;
    }
    return 0x0000;
}