#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cpu.h"
#include "gpu.h"
#include "rom.h"
#include "gb_types.h"


int main() {

    printf("Hello, BeagleBone!\n");

    struct gb_s* gameboy = bootloader(TETRIS_ROM_PATH);

    //free(gameboy->rom);
    //free(gameboy);
    if(gameboy) {
        printf("Bootloader completed successfully!\n");
    }
    else {
        printf("Bootloader failed!\n");
    }

    free(gameboy->rom);
    free(gameboy);

    return 0;
}