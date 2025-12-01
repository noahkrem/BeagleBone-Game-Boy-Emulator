// joystick.h
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdbool.h>

typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;
} joystick_state_t;

/* Initialise joystick (MCP3208 over SPI).
 * Returns true on success, false on failure.
 * If it fails, joystick_poll() will just report no movement.
 */
bool joystick_init(void);

/* Read current joystick state into 'state'.
 * If joystick was not initialised, fills with all false.
 */
void joystick_poll(joystick_state_t *state);

/* Close joystick resources (SPI fd). */
void joystick_shutdown(void);

#endif // JOYSTICK_H
