#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cpu.h"
#include "gpu.h"
#include "rom.h"
#include "gb_types.h"


int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Usage: %s <rom_file.gb>\n", argv[0]);
        return 1;
    }

    // Load ROM
    struct gb_s *gb = bootloader(argv[1]);
    if (!gb) {
        fprintf(stderr, "Failed to load ROM\n");
        return 1;
    }

    // Run emulator
    while (!gb->gb_halt) {
        cpu_step(gb);
    }

    // Clean up
    bootloader_cleanup();
    free(gb);
    
    return 0;
}