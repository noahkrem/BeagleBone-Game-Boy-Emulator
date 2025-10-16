/* 
Name: led.c 
Description: Functions to control the user LEDs on the BeagleBone
*/

#include "hal/led.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


// ----------------------
// Global variables
// ----------------------

const char ACT_LED_TRIGGER_FILEPATH[] = "/sys/class/leds/ACT/trigger";
const char ACT_LED_BRIGHTNESS_FILEPATH[] = "/sys/class/leds/ACT/brightness";
const char PWR_LED_TRIGGER_FILEPATH[] = "/sys/class/leds/PWR/trigger";
const char PWR_LED_BRIGHTNESS_FILEPATH[] = "/sys/class/leds/PWR/brightness";

// ----------------------
// Function definitions
// ----------------------

// Set the LED trigger to "none" so we can control it manually
void setup_trigger_control() {

    // ----------TRIGGER CONTROL FOR ACT LED----------

    // Open the LED trigger file for writing
    FILE *pLedTriggerFile = fopen(ACT_LED_TRIGGER_FILEPATH, "w");

    // Check if the file was opened successfully
    if (pLedTriggerFile == NULL) {
        perror("Error opening LED trigger file");
        exit(EXIT_FAILURE);
    }

    // Write the required trigger value to the file
    int charWritten = fprintf(pLedTriggerFile, "none");
    if (charWritten <= 0) {
        perror("Error writing data to LED trigger file");
        exit(EXIT_FAILURE);
    }

    // Close the file
    fclose(pLedTriggerFile);

    // ----------TRIGGER CONTROL FOR PWR LED----------

    // Open the LED trigger file for writing
    pLedTriggerFile = fopen(PWR_LED_TRIGGER_FILEPATH, "w");
    
    // Check if the file was opened successfully
    if (pLedTriggerFile == NULL) {
        perror("Error opening LED trigger file");
        exit(EXIT_FAILURE);
    }

    // Write the required trigger value to the file
    charWritten = fprintf(pLedTriggerFile, "none");
    if (charWritten <= 0) {
        perror("Error writing data to LED trigger file");
        exit(EXIT_FAILURE);
    }

    // Close the file
    fclose(pLedTriggerFile);
} 

// Set the LED brightness to on or off based on the 'on' parameter
void led_brightness(const char led_filepath[], bool on) {

    // Open the LED brightness file for writing
    FILE *pLedBrightnessFile = fopen(led_filepath, "w");

    // Check if the file was opened successfully
    if (pLedBrightnessFile == NULL) {
        perror("Error opening LED brightness file");
        exit(EXIT_FAILURE);
    }

    // Write the required brightness value to the file
    // If 'on' is true, write "1", else write "0"
    if (on) {
        int charWritten = fprintf(pLedBrightnessFile, "1");
        if (charWritten <= 0) {
            perror("Error writing data to LED brightness file");
            exit(EXIT_FAILURE);
        }
    } else {
        int charWritten = fprintf(pLedBrightnessFile, "0");
        if (charWritten <= 0) {
            perror("Error writing data to LED brightness file");
            exit(EXIT_FAILURE);
        }
    }

    // Close the file
    fclose(pLedBrightnessFile);
}


// Turn the green LED off
void act_led_off() {
    led_brightness(ACT_LED_BRIGHTNESS_FILEPATH, false);
}

// Turn the green LED on
void act_led_on() {
    led_brightness(ACT_LED_BRIGHTNESS_FILEPATH, true);
}

// Turn the red LED off
void pwr_led_off() {
    led_brightness(PWR_LED_BRIGHTNESS_FILEPATH, false);
}

// Turn the red LED on
void pwr_led_on() {
    led_brightness(PWR_LED_BRIGHTNESS_FILEPATH, true);
}


