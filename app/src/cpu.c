#include "cpu.h"
#include "registers.h"
#include "gb_types.h"
#include <stdio.h>
#include <stdlib.h>

uint16_t cpu_step(struct gb_s* gb){

    gb->gb_halt = 1; // delete this line after testing

    uint8_t opcode = 0x00; // fetch the opcode from pc address

    uint8_t middle3 = (opcode >> 3) & 0x7;
    uint8_t last3 = opcode & 0x7;

    switch(opcode >> 6){
        case 0x0: // 00******
            switch(last3){
                case 0x0:
                    switch(middle3){
                        case 0x0: // 00000000 NOP
                            break;
                        case 0x1: // 00001000 ld sp imm16
                            /* TODO */
                            break;
                        case 0x2: // 00010000 stop
                            /* TODO? */
                            break;
                        case 0x3: // 00011000 jr imm8
                            /* TODO */
                            break;
                        default: // 001**000 jr cond imm8
                            /* TODO */
                            break;
                    }
                    break;

                case 0x1:
                    if(middle3 & 0x1){ // 00**1001 add hl r16
                        /* TODO */
                    } else { // 00**0001 ld r16 imm16
                        /* TODO */
                    }
                    break;
                
                case 0x2:
                    if(middle3 & 0x1){ // 00**1010 ld a r16mem
                        /* TODO */
                    } else { // 00**0010 ld r16mem a
                        /* TODO */
                    }                    
                    break;
                
                case 0x3:
                    if(middle3 & 0x1){ // 00**1011 dec r16
                        /* TODO */
                    } else { // 00**0011 inc r16
                        /* TODO */
                    }                          
                    break;

                case 0x4: // 00***100 inc r8
                    /* TODO */
                    break;

                case 0x5: // 00***101 dec r8
                    /* TODO */
                    break;

                case 0x6: // 00***100 ld r8 imm8
                    /* TODO */
                    break;
                
                case 0x7:
                    switch(middle3){
                        case 0x1: // 000000111 rlca
                            /* TODO */
                            break;
                        case 0x2: // 000001111 rrca
                            /* TODO */
                            break;
                        case 0x3: // 000010111 rla
                            /* TODO */
                            break;
                        case 0x4: // 000011111 rra
                            /* TODO */
                            break;
                        case 0x5: // 000100111 daa
                            /* TODO */
                            break;
                        case 0x6: // 000101111 cpl
                            /* TODO */
                            break;
                        case 0x7: // 000110111 scf
                            /* TODO */
                            break;
                        case 0x8: // 000111111 ccf
                            /* TODO */
                            break;
                        default: break;
                    }
            }
            break;
        
        case 0x1: // 01******
            if(middle3 == last3){ // 01110110 ld hl hl
                /* TODO */
            } else { // ld r8 r8
                /* TODO */
            }
            break;
        
        case 0x2: // 10******
            switch(middle3){
                case 0x0: // 10000*** add a r8
                    /* TODO */
                    break;
                case 0x1: // 10001*** adc a r8
                    /* TODO */
                    break;
                case 0x2: // 10010*** sub a r8
                    /* TODO */
                    break;
                case 0x3: // 10011*** sbc a r8
                    /* TODO */
                    break;
                case 0x4: // 10100*** and a r8
                    /* TODO */
                    break;
                case 0x5: // 10101*** xor a r8
                    /* TODO */
                    break;
                case 0x6: // 10110*** or a r8
                    /* TODO */
                    break;
                case 0x7: // 10111*** cp a r8
                    /* TODO */
                    break;
                default: break;
            }
            break;
        
        case 0x3: // 11******
            switch(last3){
                case 0x0: // 11***000
                    switch(middle3){
                        case 0x4: // 11100000 ldh imm8 a
                            /* TODO */
                            break;
                        case 0x5: // 11101000 add sp imm8
                            /* TODO */
                            break;
                        case 0x6: // 11110000 ldh a imm8
                            /* TODO */
                            break;
                        case 0x7: // 11111000 ldh hl sp+imm8
                            /* TODO */
                            break;
                        default: // 110**000 ret cond
                            /* TODO */
                            break;
                    }
                    break;

                case 0x1: // 11***001
                    switch(middle3){
                        case 0x1: // 11001001 ret
                            /* TODO */
                            break;
                        case 0x3: // 11011001 reti
                            /* TODO */
                            break;
                        case 0x5: // 11101000 jp hl
                            /* TODO */
                            break;
                        case 0x7: // 11111001 ld sp hl
                            /* TODO */
                            break;
                        default: // 11**0001 pop r16stk
                            /* TODO */
                            break;
                    }
                    break;

                case 0x2: // 11***010
                    switch(middle3){
                        case 0x4: // 11100010 ldh c a
                            /* TODO */
                            break;
                        case 0x5: // 11101010 ld imm16 a
                            /* TODO */
                            break;
                        case 0x6: // 11110010 ldh a c
                            /* TODO */
                            break;
                        case 0x7: // 11111010 ld a imm16
                            /* TODO */
                            break;
                        default: // 110**010 jp cond imm16
                            /* TODO */
                            break;
                    }
                    break;
                
                case 0x3:
                    switch(middle3){
                        case 0x0: // 11000011 jp imm16
                            /* TODO */
                            break;
                        case 0x1: // 11001011 ?
                            /* TODO? */
                            break;
                        case 0x6: // 11110011 di
                            /* TODO */
                            break;
                        case 0x7: // 11111011 ei
                            /* TODO */
                            break;
                        default: break;
                    }
                    break;
                
                case 0x4: // 110**100 call cond imm16
                    /* TODO */
                    break;
                
                case 0x5: // 11***101
                    if(middle3 == 0x1){ // 11001101 call imm16
                        /* TODO */
                    } else { // 11**0101 push r16stk
                        /* TODO */
                    }
                    break;

                case 0x6: // 11***110
                    switch(middle3){
                        case 0x0: // 10000110 add a imm
                            /* TODO */
                            break;
                        case 0x1: // 10001110 adc a imm
                            /* TODO */
                            break;
                        case 0x2: // 10010110 sub a imm
                            /* TODO */
                            break;
                        case 0x3: // 10011110 sbc a imm
                            /* TODO */
                            break;
                        case 0x4: // 10100110 and a imm
                            /* TODO */
                            break;
                        case 0x5: // 10101110 xor a imm
                            /* TODO */
                            break;
                        case 0x6: // 10110110 or a imm
                            /* TODO */
                            break;
                        case 0x7: // 10111110 cp a imm
                            /* TODO */
                            break;
                        default: break;
                    }
                    break;
                
                case 0x7: // 11***111 rst tgt3
                    /* TODO */
                    break;
            }
            break;
        
        default: break;
    }

    return 0;
}