/**
 * gb_types.h - Core types and structures for Game Boy Emulator
 * Author: Noah Kremler
 * Date: 2024-06-15
 * 
 * Defines fundamental types, enums, and structures used across the emulator.
 * 
 */

#ifndef GB_TYPES_H
#define GB_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration
struct gb_s;

// -------------------------------
// Error and Status Enums
// -------------------------------

// Error types for emulator operations
enum gb_error_e {
    GB_ERROR_NONE = 0,
    GB_ERROR_INVALID_OPCODE,
    GB_INVALID_READ,
    GB_INVALID_WRITE,
    GB_INVALID_MAX
}; 

// Initialization error types that can occur during emulator startup
enum gb_init_error_e {
    GB_INIT_NO_ERROR = 0,
    GB_INIT_CARTRIDGE_UNSUPPORTED,
    GB_INIT_INVALID_CHECKSUM,
    GB_INIT_INVALID_MAX
};

// -------------------------------
// Memory Size Constants
// -------------------------------

#define WRAM_SIZE       0x2000  // 8KB Work RAM
#define VRAM_SIZE       0x2000  // 8KB Video RAM 
#define OAM_SIZE        0x00A0  // 160 bytes OAM (sprite attributes)
#define HRAM_IO_SIZE    0x0100  // 256 bytes High RAM + I/O

#define ROM_BANK_SIZE   0x4000  /* 16KB ROM bank */
#define CRAM_BANK_SIZE  0x2000  /* 8KB Cart RAM bank */

// -------------------------------
// I/O Register Addresses (Offset from 0xFF00)
// - This is because the I/O registers are mapped from 0xFF00 to 0xFFFF
// -------------------------------

#define IO_JOYP     0x00    // Joypad input
#define IO_DIV      0x04    // Divider register
#define IO_IF       0x0F    // Interrupt flag
#define IO_LCDC     0x40    // LCD control
#define IO_STAT     0x41    // LCD status

// Scroll registers
// The Game Boy LCD screen is 160x144 pixels,but the background map is 256x256 pixels.
//   The SCX and SCY registers define the top-left corner of the visible screen within this larger background map.
#define IO_SCY      0x42    // Scroll Y: How far down in the background map the screen starts
#define IO_SCX      0x43    // Scroll X: How far right in the background map the screen starts

#define IO_LY       0x44    // LCD Y coordinate: The current scanline being drawn
#define IO_LYC      0x45    // LY compare: A value to compare against LY, triggering an interrupt when they match
#define IO_DMA      0x46    // DMA transfer
#define IO_BGP      0x47    // BG palette
#define IO_OBP0     0x48    // OBJ palette 0 
#define IO_OBP1     0x49    // OBJ palette 1
#define IO_WY       0x4A    // Window Y
#define IO_WX       0x4B    // Window X 
#define IO_IE       0xFF    // Interrupt enable

// -------------------------------
// LCD Constants
// -------------------------------

// Core timing models and dimensions
// The LCD controller processes 154 scanlines per frame, with each scanline taking 456 clock cycles.
//   The full cycle happens approximately 60 times per second (59.73 Hz to be precise).
//   This defines the framerate for the original Game Boy.
#define LCD_WIDTH           160     // Physical screen width in pixels
#define LCD_HEIGHT          144     // Physical screen height in pixels
#define LCD_VERT_LINES      154     // Total scanlines including vblank, which is 10 lines
#define LCD_LINE_CYCLES     456     // Cycles per scanline

// LCD Modes
#define LCD_MODE_HBLANK     0
#define LCD_MODE_VBLANK     1
#define LCD_MODE_OAM_SCAN   2
#define LCD_MODE_LCD_DRAW   3

// LCDC bits
#define LCDC_ENABLE         0x80
#define LCDC_WINDOW_MAP     0x40
#define LCDC_WINDOW_ENABLE  0x20
#define LCDC_TILE_SELECT    0x10
#define LCDC_BG_MAP         0x08
#define LCDC_OBJ_SIZE       0x04
#define LCDC_OBJ_ENABLE     0x02
#define LCDC_BG_ENABLE      0x01

// STAT bits
#define STAT_LYC_INTR       0x40
#define STAT_MODE_2_INTR    0x20
#define STAT_MODE_1_INTR    0x10
#define STAT_MODE_0_INTR    0x08
#define STAT_LYC_COINC      0x04
#define STAT_MODE           0x03

// Sprite attributes
#define OBJ_PRIORITY        0x80
#define OBJ_FLIP_Y          0x40
#define OBJ_FLIP_X          0x20
#define OBJ_PALETTE         0x10

#define NUM_SPRITES         40      // Total sprites in OAM 
#define MAX_SPRITES_LINE    10      // Max sprites per scanline

// -------------------------------
// Interrupt Flags
// -------------------------------

#define VBLANK_INTR     0x01
#define LCDC_INTR       0x02
#define TIMER_INTR      0x04
#define SERIAL_INTR     0x08
#define CONTROL_INTR    0x10

// -------------------------------
// Timing Constants
// -------------------------------

#define DIV_CYCLES      256     // DIV increments every 256 cycles

// -------------------------------
// CPU Register Structure
// -------------------------------

struct cpu_registers_s {
    
    // Define specific bits of the flag register
    // The AF register is split into A (accumulator) and F (flags), 8 bits each
    union {
        struct {
            uint8_t  : 4; // Unused lower 4 bits
            uint8_t c: 1; // Carry flag
            uint8_t h: 1; // Half-carry flag
            uint8_t n: 1; // Subtract flag
            uint8_t z: 1; // Zero flag            
        } f_bits;
        uint8_t reg;
    } f; // Flag register
    uint8_t a; // Accumulator register
    
    // BC register pair
    union {
        struct {
            uint8_t c; // Lower byte
            uint8_t b; // Upper byte
        } bytes;
        uint16_t reg;
    } bc;

    // DE register pair
    union {
        struct {
            uint8_t e; // Lower byte
            uint8_t d; // Upper byte
        } bytes;
        uint16_t reg;
    } de;
    
    // HL register pair
    union {
        struct {
            uint8_t l; // Lower byte
            uint8_t h; // Upper byte
        } bytes;
        uint16_t reg;
    } hl;
    
    // Stack Pointer
    union {
        struct {
            uint8_t p; // Lower byte
            uint8_t s; // Upper byte
        } bytes;
        uint16_t reg;
    } sp;
    
    // Program Counter
    union {
        struct {
            uint8_t c; // Lower byte
            uint8_t p; // Upper byte
        } bytes;
        uint16_t reg;
    } pc;
};

// -------------------------------
// Timing Counters Structure (Simplified for Minimum Viable Product)
// -------------------------------

// Timing counters
struct counter_s {
    uint16_t lcd_count; // LCD timing counter
    uint16_t div_count; // Divider timing counter
};

// -------------------------------
// Display State
// -------------------------------

struct display_s {
    /**
    * Callback to draw a line on the screen
    * Called once per scanline during LCD draw mode
    * 
    * @param gb        Emulator context
    * @param pixels    160 pixels for the line
    *                  Bits 0-1: Color value (0-3)
    *                  Bits 4-5: Palette (0=OBJ0, 1=OBJ1, 2=BG)
    *                  Other bits are undefined.
    *                  Bits 4-5 are only required by frontends that support multiple palettes,
    *                  like the Game Boy Color, otherwise they can be ignored.
    * 
    * @param line      Y-coordinate (0-143)
    */
    void (*lcd_draw_line)(struct gb_s* gb, const uint8_t* pixels, uint8_t line);
    
    // Palette data
    uint8_t bg_palette[4];  // Background palette (4 colors)
    uint8_t sp_palette[8];  // Sprite palettes (2 palettes, 4 colors each)
    
    // Window tracking
    uint8_t window_clear;   // Window line counter
    uint8_t WY;             // Window Y position
};

// -------------------------------
// Main Emulator Context
// -------------------------------

struct gb_s {
    
    /**
    * Read byte from ROM
    * @param gb    Emulator context
     * @param addr  16-bit address to read from
     * @return      Byte at address
     */
     uint8_t (*gb_rom_read)(struct gb_s*, const uint32_t addr);
     
     /**
     * Read byte from cartridge RAM
     * @param gb    Emulator context
     * @param addr  16-bit address to read from
     * @return      Byte at address
     */
    uint8_t (*gb_cart_ram_read)(struct gb_s*, const uint32_t addr);

    /**
     * Write byte to cartridge RAM
     * @param gb    Emulator context
     * @param addr  Address to write to
     * @param val   Value to write
     */
    void (*gb_cart_ram_write)(struct gb_s*, const uint32_t addr, const uint8_t val);

    /**
     * Error handler
     * @param gb    Emulator context
     * @param error Error code
     * @param addr  Address where error occurred
     */
    void (*gb_error)(struct gb_s*, const enum gb_error_e error, const uint16_t addr);

    // ----- CPU State -----

    struct cpu_registers_s cpu_reg;
    
    bool gb_halt    : 1;        // CPU is halted
    bool gb_ime     : 1;        // Interrupt master enable
    bool gb_frame   : 1;        // Frame complete flag
    bool lcd_blank  : 1;        // LCD was just enabled

    // ----- Cartridge Info (MBC1 only for MVP) -----

    uint8_t mbc;                    // MBC type (0=none, 1=MBC1)
    uint8_t cart_ram;               // 1 if cartridge has RAM
    uint16_t num_rom_banks_mask;    // Mask for ROM bank selection 
    uint8_t num_ram_banks;          // Number of RAM banks 
    
    uint16_t selected_rom_bank; // Current ROM bank
    uint8_t cart_ram_bank;      // Current RAM bank
    uint8_t enable_cart_ram;    // Cart RAM enable flag
    uint8_t cart_mode_select;   // MBC1 mode select

    // ----- Timing -----

    struct counter_s counter;

    // Frame debug counter (for logging)
    uint32_t frame_debug;

    // ----- Memory Arrays -----
    
    uint8_t wram[WRAM_SIZE];        // Work RAM
    uint8_t vram[VRAM_SIZE];        // Video RAM 
    uint8_t oam[OAM_SIZE];          // Sprite attribute memory
    uint8_t hram_io[HRAM_IO_SIZE];  // High RAM and I/O registers

    // ----- Display -----

    struct display_s display;

    // ----- Direct Access -----
    // Can be modified by front-end

    struct {
        // Joypad state - set by front-end
        union {
            struct {
                bool a      : 1;
                bool b      : 1;
                bool select : 1;
                bool start  : 1;
                bool right  : 1;
                bool left   : 1;
                bool up     : 1;
                bool down   : 1;
            } joypad_bits;
            uint8_t joypad;
        };
        
        // User-defined data pointer
        void *priv;
    } direct;
};

// -------------------------------
// Local Helper Macros
// - These show up often in embedded systems
// - Simple macros for computing the minimum or maximum of two values
// -------------------------------

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// Combine two bytes into 16-bit value (little endian)
#define U8_TO_U16(hi, lo) (((uint16_t)(hi) << 8) | (lo))

#endif // GB_TYPES_H