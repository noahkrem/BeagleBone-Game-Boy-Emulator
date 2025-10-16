#ifndef LED_H
#define LED_H

#include <stdint.h>
#include <stdbool.h>


// ----------------------
// Global variables
// ----------------------

extern const char ACT_LED_TRIGGER_FILEPATH[];
extern const char ACT_LED_BRIGHTNESS_FILEPATH[];
extern const char PWR_LED_TRIGGER_FILEPATH[];
extern const char PWR_LED_BRIGHTNESS_FILEPATH[];


// ----------------------
// Function prototypes
// ----------------------

void setup_trigger_control();

void led_brightness(const char led_filepath[], bool on);

void act_led_off();

void act_led_on();

void pwr_led_off();

void pwr_led_on();


#endif // LED_H