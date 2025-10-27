#include "cpu.h"
#include "registers.h"
#include "gb_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint8_t gb_read(struct gb_s* gb, const uint32_t addr){
    /* TODO */
    return gb->wram[addr];
}

void gb_write(struct gb_s* gb, uint32_t addr, const uint8_t val){
    /* TODO */
    gb->wram[addr] = val;
}

uint16_t cpu_step(struct gb_s* gb){

    uint8_t opcode = gb_rom_read(gb, gb->cpu_reg.pc.reg++); // fetch the opcode from pc address

    uint8_t middle3 = (opcode >> 3) & 0x7; // middle 3 bits **XXX***
    uint8_t last3 = opcode & 0x7; // last 3 bits *****XXX

    switch(opcode >> 6){
        case 0x0: // 00******
            switch(last3){
                case 0x0: // 00***000
                    switch(middle3){
                        case 0x0: // 00000000 NOP
                            break;
                        
                        case 0x1:{ // 00001000 ld imm16 sp
                            uint8_t h = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            uint8_t l = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            uint16_t address = U8_TO_U16(h,l);
                            gb_write(gb, address, gb->cpu_reg.sp.bytes.s);
                            gb_write(gb, address++, gb->cpu_reg.sp.bytes.p);
                            break;
                        }
                        case 0x2: // 00010000 stop
                            gb->gb_halt = 1;
                            break;
                        
                        case 0x3: // 00011000 jr imm8
                            gb->cpu_reg.pc.reg += (int8_t) gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            break;
                        
                        default: // 001**000 jr cond imm8
                            switch(middle3 & 0x3){
                                case 0x0: 
                                    if(!gb->cpu_reg.f.f_bits.z){
                                        gb->cpu_reg.pc.reg += (int8_t) gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                                    } else {
                                        gb->cpu_reg.pc.reg++;
                                    }
                                    break;

                                case 0x1:
                                    if(gb->cpu_reg.f.f_bits.z){
                                        gb->cpu_reg.pc.reg += (int8_t) gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                                    } else {
                                        gb->cpu_reg.pc.reg++;
                                    }
                                    break;

                                case 0x2:
                                    if(!gb->cpu_reg.f.f_bits.c){
                                        gb->cpu_reg.pc.reg += (int8_t) gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                                    } else {
                                        gb->cpu_reg.pc.reg++;
                                    }
                                    break;

                                case 0x3:
                                    if(gb->cpu_reg.f.f_bits.c){
                                        gb->cpu_reg.pc.reg += (int8_t) gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                                    } else {
                                        gb->cpu_reg.pc.reg++;
                                    }
                                    break;

                                default: break;
                            }
                            break;
                    }
                    break;

                case 0x1:
                    if(middle3 & 0x1){ // 00**1001 add hl r16
                        uint16_t hl = regGet16(gb, 0x2);
                        uint16_t src = regGet16(gb, middle3 >> 1);
                        uint_fast32_t temp = hl + src;
                        gb->cpu_reg.f.f_bits.n = 0;
                        gb->cpu_reg.f.f_bits.h =
                            ((gb->cpu_reg.hl.reg & 0xFFF) + (gb->cpu_reg.sp.reg & 0xFFF)) & 0x1000 ? 1 : 0;
                        gb->cpu_reg.f.f_bits.c = temp & 0x10000 ? 1 : 0;
                        gb->cpu_reg.hl.reg = (uint16_t)temp;
                    } else { // 00**0001 ld r16 imm16
                        uint8_t h = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                        uint8_t l = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                        uint16_t imm = U8_TO_U16(h,l);
                        regSet16(gb, middle3 >> 1, imm);
                    }
                    break;
                
                case 0x2:
                    if(middle3 & 0x1){ // 00**1010 ld a r16mem
                        uint16_t address = regGet16mem(gb, middle3 >> 1);
                        gb->cpu_reg.a = gb_read(gb, address);
                    } else { // 00**0010 ld r16mem a
                        uint16_t address = regGet16mem(gb, middle3 >> 1);
                        gb_write(gb, address, regGet8(gb, gb->cpu_reg.a));
                    }                    
                    break;
                
                case 0x3:
                    if(middle3 & 0x1){ // 00**1011 dec r16
                        regSet16(gb, middle3 >> 1, regGet16(gb, middle3 >> 1) - 1);
                    } else { // 00**0011 inc r16
                        regSet16(gb, middle3 >> 1, regGet16(gb, middle3 >> 1) + 1);
                    }                          
                    break;

                case 0x4: // 00***100 inc r8
                    regSet8(gb, middle3, regGet8(gb, middle3) + 1); /* !!OPTIMIZE!! */
                    break;

                case 0x5: // 00***101 dec r8
                    regSet8(gb, middle3, regGet8(gb, middle3) - 1); /* !!OPTIMIZE!! */
                    break;

                case 0x6: {// 00***100 ld r8 imm8
                    uint8_t imm = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                    regSet8(gb, middle3, imm);
                    break;
                }
                case 0x7:
                    switch(middle3){
                        case 0x1: // 00000111 rlca
                            gb->cpu_reg.a = (gb->cpu_reg.a << 1) | (gb->cpu_reg.a >> 7);
                            gb->cpu_reg.f.reg = 0;
		                    gb->cpu_reg.f.f_bits.c = (gb->cpu_reg.a & 0x01);
                            break;
                        
                        case 0x2: // 00001111 rrca
                            gb->cpu_reg.f.reg = 0;
                            gb->cpu_reg.f.f_bits.c = gb->cpu_reg.a & 0x01;
                            gb->cpu_reg.a = (gb->cpu_reg.a >> 1) | (gb->cpu_reg.a << 7);
                            break;
                        
                        case 0x3:{ // 00010111 rla
                            uint8_t temp = gb->cpu_reg.a;
                            gb->cpu_reg.a = (gb->cpu_reg.a << 1) | gb->cpu_reg.f.f_bits.c;
                            gb->cpu_reg.f.reg = 0;
                            gb->cpu_reg.f.f_bits.c = (temp >> 7) & 0x01;
                            break;
                        }
                        case 0x4:{ // 00011111 rra
                            uint8_t temp = gb->cpu_reg.a;
                            gb->cpu_reg.a = gb->cpu_reg.a >> 1 | (gb->cpu_reg.f.f_bits.c << 7);
                            gb->cpu_reg.f.reg = 0;
                            gb->cpu_reg.f.f_bits.c = temp & 0x1;
                            break;
                        }
                        case 0x5:{ // 00100111 daa
                            int16_t a = gb->cpu_reg.a;

                            if(gb->cpu_reg.f.f_bits.n){
                                if(gb->cpu_reg.f.f_bits.h) a = (a - 0x06) & 0xFF;
                                if(gb->cpu_reg.f.f_bits.c) a -= 0x60;
                            } else {
                                if(gb->cpu_reg.f.f_bits.h || (a & 0x0F) > 9) a += 0x06;
                                if(gb->cpu_reg.f.f_bits.c || a > 0x9F) a += 0x60;
                            }

                            if((a & 0x100) == 0x100) gb->cpu_reg.f.f_bits.c = 1;

                            gb->cpu_reg.a = a;
                            gb->cpu_reg.f.f_bits.z = (gb->cpu_reg.a == 0);
                            gb->cpu_reg.f.f_bits.h = 0;
                            break;
                        }
                        case 0x6: // 00101111 cpl
                            gb->cpu_reg.a = ~gb->cpu_reg.a;
                            gb->cpu_reg.f.f_bits.n = 1;
                            gb->cpu_reg.f.f_bits.h = 1;
                            break;
                        
                        case 0x7: // 00110111 scf
                            gb->cpu_reg.f.f_bits.n = 0;
                            gb->cpu_reg.f.f_bits.h = 0;
                            gb->cpu_reg.f.f_bits.c = 1;
                            break;
                        
                        case 0x8: // 00111111 ccf
                            gb->cpu_reg.f.f_bits.n = 0;
                            gb->cpu_reg.f.f_bits.h = 0;
                            gb->cpu_reg.f.f_bits.c = ~gb->cpu_reg.f.f_bits.c;
                            break;
                        
                        default: break;
                    }
            }
            break;
        
        case 0x1: // 01******
            if(middle3 == last3){ // 01110110 ld hl hl
                /* TODO */
            } else { // ld r8 r8
                regSet8(gb, middle3, regGet8(gb, last3));
            }
            break;
        
        case 0x2: // 10******
            switch(middle3){
                case 0x0:{ // 10000*** add a r8
                    CPU_ADC_R8(regGet8(gb, last3), 0);
                    break;				
                }
                case 0x1:{ // 10001*** adc a r8
                    CPU_ADC_R8(regGet8(gb, last3), gb->cpu_reg.f.f_bits.c);
                    break;
                }			
                case 0x2:{ // 10010*** sub a r8
                    CPU_SBC_R8(regGet8(gb, last3), 0);
                    break;
                }
                case 0x3:{ // 10011*** sbc a r8
                    CPU_SBC_R8(regGet8(gb, last3), gb->cpu_reg.f.f_bits.c);
                    break;
                }
                case 0x4: // 10100*** and a r8
                    CPU_AND_R8(regGet8(gb, last3));
                    break;
                
                case 0x5: // 10101*** xor a r8
                    CPU_XOR_R8(regGet8(gb, last3));
                    break;
                
                case 0x6: // 10110*** or a r8
                    CPU_OR_R8(regGet8(gb, last3));
                    break;

                case 0x7: // 10111*** cp a r8
                    CPU_CP_R8(regGet8(gb, last3));
                    break;

                default: break;
            }
            break;
        
        case 0x3: // 11******
            switch(last3){
                case 0x0: // 11***000
                    switch(middle3){
                        case 0x4: // 11100000 ldh imm8 a
                            gb_write(gb, 0xFF00 | gb_rom_read(gb, gb->cpu_reg.pc.reg++), gb->cpu_reg.a);
                            break;
                        
                        case 0x5:{ // 11101000 add sp imm8
                            int8_t imm = (int8_t) gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            gb->cpu_reg.f.reg = 0;
                            gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (imm & 0xF) > 0xF) ? 1 : 0;
                            gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (imm & 0xFF) > 0xFF);
                            gb->cpu_reg.sp.reg += imm;
                            break;
                        }
                        case 0x6: // 11110000 ldh a imm8
                            gb->cpu_reg.a = gb_read(gb, 0xFF00 | gb_rom_read(gb, gb->cpu_reg.pc.reg++));
                            break;
                        
                        case 0x7:{ // 11111000 ldh hl sp+imm8
                            int8_t imm = (int8_t) gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            gb->cpu_reg.hl.reg = gb->cpu_reg.sp.reg + imm;
                            gb->cpu_reg.f.reg = 0;
                            gb->cpu_reg.f.f_bits.h = ((gb->cpu_reg.sp.reg & 0xF) + (imm & 0xF) > 0xF) ? 1 : 0;
                            gb->cpu_reg.f.f_bits.c = ((gb->cpu_reg.sp.reg & 0xFF) + (imm & 0xFF) > 0xFF) ? 1 : 0;
                            break;
                        }
                        default: // 110**000 ret cond
                            switch(middle3){
                                case 0x0: 
                                    if(!gb->cpu_reg.f.f_bits.z){
                                        gb->cpu_reg.pc.bytes.c = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                                        gb->cpu_reg.pc.bytes.p = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                                    }
                                    break;

                                case 0x1:
                                    if(gb->cpu_reg.f.f_bits.z){
                                        gb->cpu_reg.pc.bytes.c = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                                        gb->cpu_reg.pc.bytes.p = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                                    }
                                    break;

                                case 0x2:
                                    if(!gb->cpu_reg.f.f_bits.c){
                                        gb->cpu_reg.pc.bytes.c = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                                        gb->cpu_reg.pc.bytes.p = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                                    }
                                    break;

                                case 0x3:
                                    if(gb->cpu_reg.f.f_bits.c){
                                        gb->cpu_reg.pc.bytes.c = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                                        gb->cpu_reg.pc.bytes.p = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                                    }
                                    break;

                                default: break;
                            }
                            break;
                    }
                    break;

                case 0x1: // 11***001
                    switch(middle3){
                        case 0x1: // 11001001 ret
                            gb->cpu_reg.pc.bytes.c = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                            gb->cpu_reg.pc.bytes.p = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                            break;

                        case 0x3: // 11011001 reti
                            gb->cpu_reg.pc.bytes.c = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                            gb->cpu_reg.pc.bytes.p = gb_rom_read(gb, gb->cpu_reg.sp.reg++);
                            gb->gb_ime = true;
                            break;
                        
                        case 0x5: // 11101001 jp hl
                            gb->cpu_reg.pc.reg = gb->cpu_reg.hl.reg;
                            break;

                        case 0x7: // 11111001 ld sp hl
                            gb->cpu_reg.sp.reg = gb->cpu_reg.hl.reg;
                            break;
                        
                        default:{ // 11**0001 pop r16stk
                            /* FIX */
                            uint8_t h = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            uint8_t l = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            uint16_t imm = U8_TO_U16(h,l);
                            regSet16stk(gb, middle3 >> 1, imm);
                            break;
                        }
                    }
                    break;

                case 0x2: // 11***010
                    switch(middle3){
                        case 0x4: // 11100010 ldh c a
                            gb_write(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c, gb->cpu_reg.a);
                            break;
                        
                        case 0x5:{ // 11101010 ld imm16 a
                            uint8_t h = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            uint8_t l = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            uint16_t address = U8_TO_U16(h,l);
                            gb_write(gb, address, gb->cpu_reg.a);
                            break;
                        }
                        case 0x6: // 11110010 ldh a c
                            gb->cpu_reg.a = gb_read(gb, 0xFF00 | gb->cpu_reg.bc.bytes.c);
                            break;
                        
                        case 0x7:{ // 11111010 ld a imm16
                            uint8_t h = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            uint8_t l = gb_rom_read(gb, gb->cpu_reg.pc.reg++);
                            uint16_t address = U8_TO_U16(h,l);
                            gb->cpu_reg.a = gb_read(gb, address);
                            break;
                        }
                        default: // 110**010 jp cond imm16
                            switch(middle3){
                                case 0x0: 
                                    if(!gb->cpu_reg.f.f_bits.z){
                                        gb->cpu_reg.pc.bytes.c = gb_read(gb, gb->cpu_reg.pc.reg++);
			                            gb->cpu_reg.pc.bytes.p = gb_read(gb, gb->cpu_reg.pc.reg++);
                                    } else {
                                        gb->cpu_reg.pc.reg += 2;
                                    }
                                    break;

                                case 0x1:
                                    if(gb->cpu_reg.f.f_bits.z){
                                        gb->cpu_reg.pc.bytes.c = gb_read(gb, gb->cpu_reg.pc.reg++);
			                            gb->cpu_reg.pc.bytes.p = gb_read(gb, gb->cpu_reg.pc.reg++);
                                    } else {
                                        gb->cpu_reg.pc.reg += 2;
                                    }
                                    break;

                                case 0x2:
                                    if(!gb->cpu_reg.f.f_bits.c){
                                        gb->cpu_reg.pc.bytes.c = gb_read(gb, gb->cpu_reg.pc.reg++);
			                            gb->cpu_reg.pc.bytes.p = gb_read(gb, gb->cpu_reg.pc.reg++);
                                    } else {
                                        gb->cpu_reg.pc.reg += 2;
                                    }
                                    break;
                                    
                                case 0x3:
                                    if(gb->cpu_reg.f.f_bits.c){
                                        gb->cpu_reg.pc.bytes.c = gb_read(gb, gb->cpu_reg.pc.reg++);
			                            gb->cpu_reg.pc.bytes.p = gb_read(gb, gb->cpu_reg.pc.reg++);
                                    } else {
                                        gb->cpu_reg.pc.reg += 2;
                                    }
                                    break;

                                default: break;
                            }
                            break;
                    }
                    break;
                
                case 0x3:
                    switch(middle3){
                        case 0x0: // 11000011 jp imm16
                            gb->cpu_reg.pc.bytes.c = gb_read(gb, gb->cpu_reg.pc.reg++);
                            gb->cpu_reg.pc.bytes.p = gb_read(gb, gb->cpu_reg.pc.reg++);
                            break;
                        
                        case 0x1: // 11001011 CB instructions
                            cpu_execute_cb(gb);
                            break;
                        
                        case 0x6: // 11110011 di
                            gb->gb_ime = false;
                            break;
                        
                        case 0x7: // 11111011 ei
                            gb->gb_ime = true;
                            break;

                        default: break;
                    }
                    break;
                
                case 0x4: // 110**100 call cond imm16
                    switch(middle3 & 0x3){
                        case 0x0: 
                            if(!gb->cpu_reg.f.f_bits.z){
                                uint8_t c = gb_read(gb, gb->cpu_reg.pc.reg++);
			                    uint8_t p = gb_read(gb, gb->cpu_reg.pc.reg++);
                                gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
			                    gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                                gb->cpu_reg.pc.bytes.c = c;
			                    gb->cpu_reg.pc.bytes.p = p;
                            } else {
                                gb->cpu_reg.pc.reg += 2;
                            }
                            break;

                        case 0x1:
                            if(gb->cpu_reg.f.f_bits.z){
                                uint8_t c = gb_read(gb, gb->cpu_reg.pc.reg++);
			                    uint8_t p = gb_read(gb, gb->cpu_reg.pc.reg++);
                                gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
			                    gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                                gb->cpu_reg.pc.bytes.c = c;
			                    gb->cpu_reg.pc.bytes.p = p;
                            } else {
                                gb->cpu_reg.pc.reg += 2;
                            }
                            break;

                        case 0x2:
                            if(!gb->cpu_reg.f.f_bits.c){
                                uint8_t c = gb_read(gb, gb->cpu_reg.pc.reg++);
			                    uint8_t p = gb_read(gb, gb->cpu_reg.pc.reg++);
                                gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
			                    gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                                gb->cpu_reg.pc.bytes.c = c;
			                    gb->cpu_reg.pc.bytes.p = p;
                            } else {
                                gb->cpu_reg.pc.reg += 2;
                            }
                            break;

                        case 0x3:
                            if(gb->cpu_reg.f.f_bits.c){
                                uint8_t c = gb_read(gb, gb->cpu_reg.pc.reg++);
			                    uint8_t p = gb_read(gb, gb->cpu_reg.pc.reg++);
                                gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
			                    gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                                gb->cpu_reg.pc.bytes.c = c;
			                    gb->cpu_reg.pc.bytes.p = p;
                            } else {
                                gb->cpu_reg.pc.reg += 2;
                            }
                            break;
                        default: break;
                    }
                    break;
                
                case 0x5: // 11***101
                    if(middle3 == 0x1){ // 11001101 call imm16
                        uint8_t c = gb_read(gb, gb->cpu_reg.pc.reg++);
                        uint8_t p = gb_read(gb, gb->cpu_reg.pc.reg++);
                        gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
                        gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                        gb->cpu_reg.pc.bytes.c = c;
                        gb->cpu_reg.pc.bytes.p = p;
                    } else { // 11**0101 push r16stk 
                        uint16_t reg = regGet16stk(gb, middle3 >> 1);
                        gb_write(gb, --gb->cpu_reg.sp.reg, reg >> 8);
		                gb_write(gb, --gb->cpu_reg.sp.reg, reg & 0x7);
                    }
                    break;

                case 0x6: // 11***110
                    switch(middle3){
                        case 0x0: // 10000110 add a imm
                            CPU_ADC_R8(gb_rom_read(gb, gb->cpu_reg.pc.reg++), 0);
                            break;

                        case 0x1: // 10001110 adc a imm
                            CPU_ADC_R8(gb_rom_read(gb, gb->cpu_reg.pc.reg++), gb->cpu_reg.f.f_bits.c);
                            break;

                        case 0x2: // 10010110 sub a imm
                            CPU_SBC_R8(gb_rom_read(gb, gb->cpu_reg.pc.reg++), 0);
                            break;

                        case 0x3: // 10011110 sbc a imm
                            CPU_SBC_R8(gb_rom_read(gb, gb->cpu_reg.pc.reg++), gb->cpu_reg.f.f_bits.c);
                            break;

                        case 0x4: // 10100110 and a imm
                            CPU_AND_R8(gb_rom_read(gb, gb->cpu_reg.pc.reg++));
                            break;

                        case 0x5: // 10101110 xor a imm
                            CPU_XOR_R8(gb_rom_read(gb, gb->cpu_reg.pc.reg++));
                            break;

                        case 0x6: // 10110110 or a imm
                            CPU_OR_R8(gb_rom_read(gb, gb->cpu_reg.pc.reg++));
                            break;

                        case 0x7: // 10111110 cp a imm
                            CPU_CP_R8(gb_rom_read(gb, gb->cpu_reg.pc.reg++));
                            break;

                        default: break;
                    }
                    break;
                
                case 0x7: // 11***111 rst tgt3
                    gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.p);
		            gb_write(gb, --gb->cpu_reg.sp.reg, gb->cpu_reg.pc.bytes.c);
                    gb->cpu_reg.pc.reg = 0x8 * middle3;
                    break;
            }
            break;
        
        default: break;
    }

    return 0;
}