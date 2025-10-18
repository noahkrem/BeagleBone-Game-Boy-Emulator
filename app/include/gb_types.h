/**
 * gb_types.h - Core types and structures for Game Boy Emulator
 * Author: Noah Kremler
 * Date: 2024-06-15
 * 
 * Defines fundamental types, enums, and structures used across the emulator.
 */

#ifndef GB_TYPES_H
#define GB_TYPES_H


// Forward declaration
struct gb_s;

// Error types for emulator operations
enum gb_error_e {
    GB_ERROR_NONE = 0,
    GB_ERROR_INVALID_OPCODE = 1,
    GB_INVALID_READ = 2,
    GB_INVALID_WRITE = 3,
    GB_INVALID_MAX = 4
}; 


#endif // GB_TYPES_H