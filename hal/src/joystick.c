/*
Name: joystick.c
Description: Functions to read the joystick state (X, Y, button)
*/

#include "hal/joystick.h"
#include "hal/spi_access.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <errno.h>
#include <stdlib.h>


// ----------------------
// Global variables
// ----------------------

static const uint32_t JOYSTICK_SPI_SPEED_HZ = 250000; // 250 kHz

// File-scope static variables for smoothing
static float xSmooth = 2048.0f;
static float ySmooth = 2036.0f;

// ----------------------
// Function Definitions
// ----------------------

// Helper function to normalize raw ADC values to -1.0 to 1.0 range
// This function improves the feeling of the joystick by centering around 0 and scaling appropriately
static float normalize(int raw, int min, int max) {
    float mid = (max + min) / 2.0f;
    float range = (max - min) / 2.0f;
    return (raw - mid) / range;
}


/**
 * Opens and configures the SPI joystick device.
 * 
 * Opens and configures the SPI joystick device.
 * The SPI configuration persists after these ioctl() calls,
 * so it does NOT need to be re-applied for each transfer.
 */
int joystick_open(const char *device_path) {

    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -errno;
    }

    // Open SPI device
    uint8_t mode = 0;       // SPI mode 0
    uint8_t bits = 8;       // Bits per word
    uint32_t speed = JOYSTICK_SPI_SPEED_HZ; // Max speed in Hz

    // Set SPI parameters
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1) {
        perror("SPI_IOC_WR_MODE");
        joystick_close(fd);
        return -errno;
    }

    // Set bits per word
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1) {
        perror("SPI_IOC_WR_BITS_PER_WORD");
        joystick_close(fd);
        return -errno;
    }

    // Set max speed
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("SPI_IOC_WR_MAX_SPEED_HZ");
        joystick_close(fd);
        return -errno;
    }

    return fd;
}


// Process raw joystick state
// Applies clamping, dead zone, smoothing, normalization, and direction determination
// The smoothing remembers previous values using static variables (defined above)
void joystick_process_state(JoystickState *state) {
    
    if (!state) return;

    // Clamp extremes
    if (state->x < 0 || state->x > 4095) state->x = 2048;
    if (state->y < 0 || state->y > 4095) state->y = 2036;

    // Apply dead zone around center
    const int DEAD_ZONE = 50; // adjust as needed
    if (abs(state->x - 2048) < DEAD_ZONE) state->x = 2048;
    if (abs(state->y - 2036) < DEAD_ZONE) state->y = 2036;

    // Apply exponential smoothing
    const float SMOOTHING_FACTOR = 0.2f; // 0.0 = no change, 1.0 = instant change
    xSmooth = xSmooth * (1.0f - SMOOTHING_FACTOR) + state->x * SMOOTHING_FACTOR;
    ySmooth = ySmooth * (1.0f - SMOOTHING_FACTOR) + state->y * SMOOTHING_FACTOR;

    state->x = (int)(xSmooth + 0.5f);
    state->y = (int)(ySmooth + 0.5f);

    // ---- 4. Recompute normalized values ----
    state->xNorm = normalize(state->x, 0, 4095);
    state->yNorm = normalize(state->y, 0, 4095);

    // ---- 5. Determine direction ----
    if (state->xNorm > 0.5f) {
        state->direction = JOYSTICK_DIR_RIGHT;
    } else if (state->xNorm < -0.5f) {
        state->direction = JOYSTICK_DIR_LEFT;
    } else if (state->yNorm > 0.5f) {
        state->direction = JOYSTICK_DIR_UP;
    } else if (state->yNorm < -0.5f) {
        state->direction = JOYSTICK_DIR_DOWN;
    } else {
        state->direction = JOYSTICK_DIR_NEUTRAL;
    }
}

/**
 * Read the current state of the joystick.
 * 
 * Some important notes about this function:
 * - Every ioctl() call reconfigures the SPI device, which is inefficient.
 * - That is why we have ioctl() called in joystick_open() instead of here. 
 */
int joystick_read_state(int fd, JoystickState *state) {

    // Validate inputs
    if (fd < 0 || state == NULL)
        return -EINVAL;

    // Read X and Y channels
    int ch0 = read_ch(fd, 0, JOYSTICK_SPI_SPEED_HZ);
    int ch1 = read_ch(fd, 1, JOYSTICK_SPI_SPEED_HZ);

    // Check for read errors
    if (!(ch0 >= 0 && ch1 >= 0))
        fprintf(stderr, "Failed to read from ADC channels\n");


    // Populate the JoystickState struct
    state->x = ch0;
    state->y = ch1;
    state->xNorm = normalize(ch0, 0, 4095);
    state->yNorm = normalize(ch1, 0, 4095);
    
    // Determine direction based on normalized values
    if (state->xNorm > 0.5f) {
        state->direction = JOYSTICK_DIR_RIGHT;
    } else if (state->xNorm < -0.5f) {
        state->direction = JOYSTICK_DIR_LEFT;
    } else if (state->yNorm > 0.5f) {
        state->direction = JOYSTICK_DIR_UP;
    } else if (state->yNorm < -0.5f) {
        state->direction = JOYSTICK_DIR_DOWN;
    } else {
        state->direction = JOYSTICK_DIR_NEUTRAL;
    }

    joystick_process_state(state);

    return 0;
}

// Read immediate raw state without smoothing/dead-zone processing
int joystick_read_raw_state(int fd, JoystickState *state) {
    if (fd < 0 || state == NULL)
        return -EINVAL;

    int ch0 = read_ch(fd, 0, JOYSTICK_SPI_SPEED_HZ);
    int ch1 = read_ch(fd, 1, JOYSTICK_SPI_SPEED_HZ);

    if (!(ch0 >= 0 && ch1 >= 0))
        fprintf(stderr, "Failed to read from ADC channels (raw)\n");

    state->x = ch0;
    state->y = ch1;
    state->xNorm = normalize(ch0, 0, 4095);
    state->yNorm = normalize(ch1, 0, 4095);

    if (state->xNorm > 0.5f) {
        state->direction = JOYSTICK_DIR_RIGHT;
    } else if (state->xNorm < -0.5f) {
        state->direction = JOYSTICK_DIR_LEFT;
    } else if (state->yNorm > 0.5f) {
        state->direction = JOYSTICK_DIR_UP;
    } else if (state->yNorm < -0.5f) {
        state->direction = JOYSTICK_DIR_DOWN;
    } else {
        state->direction = JOYSTICK_DIR_NEUTRAL;
    }

    return 0;
}

// Check if the joystick is centered
bool joystick_centered(const JoystickState *state) {
    if (state == NULL)
        return false;

    // Centered if direction is neutral
    return state->direction == JOYSTICK_DIR_NEUTRAL;
}

// Close the joystick (SPI device)
void joystick_close(int fd) {
    if (fd >= 0)
        close(fd);
}


// Diagnose joystick readings by printing multiple samples
// Only used for debugging
void joystick_diagnose_reading(int fd, int samples) {
    
    if (fd < 0) {
        fprintf(stderr, "Invalid file descriptor for joystick\n");
        return;
    }

    JoystickState state;
    if (joystick_read_state(fd, &state) < 0) {
        fprintf(stderr, "Failed to read joystick state\n");
        return;
    }

    printf("Diagnosing joystick readings:\n");
    for (int i = 0; i < samples; i++) {
        if (joystick_read_state(fd, &state) < 0) {
            fprintf(stderr, "Failed to read joystick state\n");
            return;
        }
        printf("Sample %d: X=%d, Y=%d, xNorm=%.2f, yNorm=%.2f, Direction=%d\n", 
               i+1, state.x, state.y, state.xNorm, state.yNorm, state.direction);
    }
}