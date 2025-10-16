#ifndef JOYSTICK_H
#define JOYSTICK_H


#include <stdbool.h>
#include <stdint.h>

// ----------------------
// Global variables
// ----------------------


// ----------------------
// Structs/Enums
// ----------------------

typedef enum {
    JOYSTICK_DIR_NEUTRAL,
    JOYSTICK_DIR_UP,
    JOYSTICK_DIR_DOWN,
    JOYSTICK_DIR_LEFT,
    JOYSTICK_DIR_RIGHT
} JoystickDirection;

// Represents the state of the joystick
// Note: could also include button press state if needed
typedef struct {
    int x;  // X-axis value (0 to 4095)
    int y;  // Y-axis value (0 to 4095)
    float xNorm; // Normalized X-axis value (-1.0 to 1.0)
    float yNorm; // Normalized Y-axis value (-1.0 to 1.0)
    JoystickDirection direction;    // Current direction
} JoystickState;

// ----------------------
// Function prototypes
// ----------------------

int joystick_open(const char *device_path);
void joystick_process_state(JoystickState *state);
int joystick_read_state(int fd, JoystickState *state);
bool joystick_centered(const JoystickState *state);
// Read joystick state without applying smoothing or the dead-zone adjustments.
// Use this for single-sample checks (e.g., "too early" or immediate centered checks).
int joystick_read_raw_state(int fd, JoystickState *state);

void joystick_close(int fd);

void joystick_diagnose_reading(int fd, int samples);


#endif // JOYSTICK_H