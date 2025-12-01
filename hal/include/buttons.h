// buttons.h
#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>

typedef struct {
    bool start;  // GPIO15
    bool a;      // GPIO16
    bool b;      // GPIO17
} buttons_state_t;

/* Initialise GPIO buttons (libgpiod v2).
 * Returns true on success, false on failure.
 * If it fails, buttons_poll() will just report all false.
 */
bool buttons_init(void);

/* Read current button state into 'state'.
 * If not initialised, fills with all false.
 */
void buttons_poll(buttons_state_t *state);

/* Release GPIO resources. */
void buttons_shutdown(void);

#endif // BUTTONS_H
