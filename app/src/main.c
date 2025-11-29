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

        // step through CPU cycles until a complete frame is ready to be displayed
        while (!gb->gb_frame) {
                cpu_step(gb);
            }
        gpu_display_frame();
    }

    // Clean up
    bootloader_cleanup();
    gpu_cleanup();
    free(gb);
    
    return 0;
}