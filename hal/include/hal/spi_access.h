#ifndef SPI_ACCESS_H
#define SPI_ACCESS_H

#include <stdint.h>

// ----------------------
// Function prototypes
// ----------------------

int read_ch(int fd, int ch, uint32_t speed_hz);

#endif // SPI_ACCESS_H