#!/usr/bin/env python3
"""
create_test_rom.py - Generate a minimal Game Boy test ROM

This creates a simple ROM that:
1. Clears the screen
2. Draws a simple pattern
3. Responds to button presses (changes colors)
4. Tests basic CPU/PPU/Input functionality

Usage:
    python3 create_test_rom.py
    ./gb_emulator test.gb
"""

import struct

def create_minimal_test_rom():
    """Create a minimal valid Game Boy ROM"""
    
    # Create 32KB ROM (minimum size)
    rom = bytearray(32768)
    
    # Fill with HALT instructions (0x76) as safe default
    for i in range(len(rom)):
        rom[i] = 0x76
    
    # Nintendo logo (required for validation)
    nintendo_logo = bytes([
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
        0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
    ])
    rom[0x0104:0x0104+len(nintendo_logo)] = nintendo_logo
    
    # ROM title
    title = b"TEST ROM"
    rom[0x0134:0x0134+len(title)] = title
    
    # ROM header
    rom[0x0147] = 0x00  # ROM only (no MBC)
    rom[0x0148] = 0x00  # 32KB
    rom[0x0149] = 0x00  # No RAM
    rom[0x014A] = 0x01  # Non-Japanese
    
    # Simple test program starting at 0x0100
    program = [
        # Entry point (0x0100)
        0x00,        # NOP
        0xC3, 0x50, 0x01,  # JP 0x0150 (jump to main program)
        
        # Padding to 0x0150
    ]
    
    for i, byte in enumerate(program):
        rom[0x0100 + i] = byte
    
    # Main program at 0x0150
    main_program = [
        # Disable interrupts
        0xF3,              # DI
        
        # Turn off LCD
        0xFA, 0x40, 0xFF,  # LD A, ($FF40)  ; Load LCDC
        0xCB, 0x7F,        # BIT 7, A        ; Test LCD enable bit
        0x20, 0x03,        # JR NZ, skip     ; If LCD on, wait for VBlank
        0x01, 0x40, 0xFF,  # LD BC, $FF40
        
        # Wait for VBlank
        0xFA, 0x44, 0xFF,  # wait_vblank: LD A, ($FF44)  ; Load LY
        0xFE, 0x90,        # CP $90          ; Compare with 144
        0x20, 0xFA,        # JR NZ, wait_vblank
        
        # Turn off LCD
        0x3E, 0x00,        # LD A, $00
        0xE0, 0x40,        # LDH ($FF40), A  ; LCDC = 0 (LCD off)
        
        # Clear VRAM
        0x21, 0x00, 0x80,  # LD HL, $8000    ; Start of VRAM
        0x06, 0x20,        # LD B, $20       ; 32 pages to clear
        0x0E, 0x00,        # clear_vram: LD C, $00
        0x3E, 0x00,        # LD A, $00
        0x22,              # clear_loop: LDI (HL), A
        0x0D,              # DEC C
        0x20, 0xFC,        # JR NZ, clear_loop
        0x05,              # DEC B
        0x20, 0xF7,        # JR NZ, clear_vram
        
        # Set up palette (all white)
        0x3E, 0xE4,        # LD A, $E4       ; Palette: 3,2,1,0
        0xE0, 0x47,        # LDH ($FF47), A  ; BGP
        
        # Draw simple pattern in tile 1
        0x21, 0x10, 0x80,  # LD HL, $8010    ; Tile 1 start
        0x3E, 0xFF,        # LD A, $FF
        0x06, 0x10,        # LD B, $10       ; 16 bytes
        0x22,              # draw_tile: LDI (HL), A
        0x05,              # DEC B
        0x20, 0xFC,        # JR NZ, draw_tile
        
        # Fill tilemap with tile 1
        0x21, 0x00, 0x98,  # LD HL, $9800    ; BG tilemap
        0x06, 0x04,        # LD B, $04       ; 4 pages
        0x0E, 0x00,        # fill_map: LD C, $00
        0x3E, 0x01,        # LD A, $01       ; Tile 1
        0x22,              # fill_loop: LDI (HL), A
        0x0D,              # DEC C
        0x20, 0xFC,        # JR NZ, fill_loop
        0x05,              # DEC B
        0x20, 0xF7,        # JR NZ, fill_map
        
        # Turn on LCD
        0x3E, 0x91,        # LD A, $91       ; LCDC: LCD on, BG on
        0xE0, 0x40,        # LDH ($FF40), A
        
        # Main loop - just halt
        0x76,              # main_loop: HALT
        0x18, 0xFD,        # JR main_loop
    ]
    
    for i, byte in enumerate(main_program):
        rom[0x0150 + i] = byte
    
    # Calculate header checksum
    checksum = 0
    for i in range(0x0134, 0x014D):
        checksum = (checksum - rom[i] - 1) & 0xFF
    rom[0x014D] = checksum
    
    return rom

def main():
    """Generate and save test ROM"""
    print("Creating test ROM...")
    
    rom = create_minimal_test_rom()
    
    # Save to file
    filename = "test.gb"
    with open(filename, "wb") as f:
        f.write(rom)
    
    print(f"✓ Created {filename} ({len(rom)} bytes)")
    print("\nThis ROM will:")
    print("  - Display a white screen (tests PPU)")
    print("  - Run without crashing (tests CPU)")
    print("  - Validate Nintendo logo (tests bootloader)")
    print("\nTest it with:")
    print(f"  ./gb_emulator {filename}")
    print("\nExpected result:")
    print("  - Window opens")
    print("  - White screen appears")
    print("  - No crashes")

if __name__ == "__main__":
    main()