// Include C standard libraries:
#include <stdlib.h> // C Standard library
#include <stdio.h>  // C Standard I/O libary
#include <time.h>   // C Standard get and manipulate time library

// Include C POSIX libraries:
#include <unistd.h>   // Symbolic constants and types library

// Include dma_pwm.c:
#include "dma_pwm.h"

// Test script
int main() {
    // Definitions:
    int channel;
    int gpio[1] = {26}; // Typically free
    float freq = 1;     // Hz
    float duty = 75;    // Percentage

    // Configure:
    if (config_pwm(DEFAULT_PAGES, LED_PULSE_WIDTH) != 0) {
        // Exit with error:
        return -1;
    };

    // Status:
    printf("dma_pwm.c configured\n");

    // Request PWM channel:
    channel = request_pwm();

    // Check success:
    if (channel < 0) {
        // Status:
        printf("Could not request channel\n");

        // Exit with error:
        return -1;
    }

    // Status:
    printf("Channel %d requested\n", channel);

    // Configure PWM signal on a channel:
    if (set_pwm(channel, gpio, (sizeof(gpio) / sizeof(int)), freq, duty) != 0) {
        // Clean-up:
        free_pwm(channel);

        // Exit with error:
        return -1;
    };

    // Status:
    printf("Channel %d PWM signal set\n", channel);

    // Get properties:
    printf("PWM signal frequency:  %0.3f Hz\n", get_freq_pwm(channel));
    printf("PWM signal duty cycle: %0.3f%%\n", get_duty_cycle_pwm(channel));

    // Enable PWM output:
    if(enable_pwm(channel) != 0) {
        // Clean-up:
        free_pwm(channel);

        // Exit with error:
        return -1;
    };

    // Status:
    printf("Channel %d enabled\n", channel);

    // Wait for some time:
    sleep(5);

    // Update properties:
    freq = 5;
    duty = 50;

    // Update channel:
    if (set_pwm(channel, gpio, (sizeof(gpio) / sizeof(int)), freq, duty) != 0) {
        // Clean-up:
        free_pwm(channel);

        // Exit with error:
        return -1;
    };

    // Status:
    printf("Channel %d PWM signal updated\n", channel);

    // Get properties:
    printf("PWM signal frequency:  %0.3f Hz\n", get_freq_pwm(channel));
    printf("PWM signal duty cycle: %0.3f %%\n", get_duty_cycle_pwm(channel));

    // Wait for some time:
    sleep(5);

    // Disable channel:
    if (disable_pwm(channel) != 0) {
        // Clean-up:
        free_pwm(channel);
        
        // Exit with error:
        return -1;
    };

    // Status:
    printf("Channel %d disabled\n", channel);

    // Clean-up:
    free_pwm(channel);

    // Status:
    printf("Channel %d freed\n", channel);

    // Exit:
    return 0;
}