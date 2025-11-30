// joystick.c  (renamed from adc_continuous.c)
#include "joystick.h"

#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <errno.h>

/* ---------- config: adjust if wiring is different ---------- */

/* SPI device for MCP3208 */
#define JOY_SPI_DEV     "/dev/spidev0.0"
#define JOY_SPI_SPEED   250000u

/* ADC channels: 0 = X, 1 = Y (change if needed) */
#define JOY_X_CH        0
#define JOY_Y_CH        1

/* 12-bit ADC range 0..4095, centre ≈ 2048 */
#define JOY_CENTER      2048
#define JOY_DEADZONE    600   /* tweak to adjust sensitivity */

/* ---------------------------------------------------------- */

static int joy_fd = -1;

/* Internal helper to read one MCP3208 channel */
static int read_ch(int fd, int ch, uint32_t speed_hz)
{
    uint8_t tx[3] = {
        (uint8_t)(0x06 | ((ch & 0x04) >> 2)),
        (uint8_t)((ch & 0x03) << 6),
        0x00
    };
    uint8_t rx[3] = {0};

    struct spi_ioc_transfer tr = {
        .tx_buf        = (unsigned long)tx,
        .rx_buf        = (unsigned long)rx,
        .len           = 3,
        .speed_hz      = speed_hz,
        .bits_per_word = 8,
        .delay_usecs   = 0,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        return -1;
    }

    int value = ((rx[1] & 0x0F) << 8) | rx[2];
    return value;
}

bool joystick_init(void)
{
    if (joy_fd >= 0)
        return true;  // already open

    joy_fd = open(JOY_SPI_DEV, O_RDWR);
    if (joy_fd < 0) {
        return false;
    }

    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = JOY_SPI_SPEED;

    if (ioctl(joy_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(joy_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(joy_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        close(joy_fd);
        joy_fd = -1;
        return false;
    }

    return true;
}

void joystick_poll(joystick_state_t *state)
{
    if (!state) return;

    /* default: no direction pressed */
    state->up = state->down = state->left = state->right = false;

    if (joy_fd < 0)
        return;   // joystick not available, leave as all false

    int x = read_ch(joy_fd, JOY_X_CH, JOY_SPI_SPEED);
    int y = read_ch(joy_fd, JOY_Y_CH, JOY_SPI_SPEED);

    if (x < 0 || y < 0) {
        return;   // read error, treat as neutral
    }

    /* Horizontal */
    if (x < JOY_CENTER - JOY_DEADZONE) {
        state->left  = true;
        state->right = false;
    } else if (x > JOY_CENTER + JOY_DEADZONE) {
        state->right = true;
        state->left  = false;
    }

    /* Vertical (flip if joystick orientation is opposite) */
    if (y < JOY_CENTER - JOY_DEADZONE) {
        state->up   = true;
        state->down = false;
    } else if (y > JOY_CENTER + JOY_DEADZONE) {
        state->down = true;
        state->up   = false;
    }
}

void joystick_shutdown(void)
{
    if (joy_fd >= 0) {
        close(joy_fd);
        joy_fd = -1;
    }
}
