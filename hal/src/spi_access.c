/*
Name: spi_access.c
Description: Functions to read from the ADC via SPI
*/

#include "hal/spi_access.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

/**
 * @brief Read one channel from the ADC via SPI
 *
 * @param fd File descriptor for SPI device
 * @param ch Channel number (0–7 depending on ADC)
 * @param speed_hz SPI clock speed in Hz
 * @return 12-bit ADC result or -1 on error
 */
int read_ch(int fd, int ch, uint32_t speed_hz) {
    
    uint8_t tx[3] = {
        (uint8_t)(0x06 | ((ch & 0x04) >> 2)),  // Start bit + single-ended bit + D2
        (uint8_t)((ch & 0x03) << 6),           // D1 + D0 bits shifted into position
        0x00                                   // Placeholder for received data
    };

    uint8_t rx[3] = { 0 };

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 3,
        .speed_hz = speed_hz,
        .bits_per_word = 8,
        .cs_change = 0
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        perror("SPI_IOC_MESSAGE");
        return -1;
    }

    // Combine the received bytes into a 12-bit result
    return ((rx[1] & 0x0F) << 8) | rx[2];
}