// DMA PWM: Direct Memory Access (DMA) PWM for the Raspberry Pi
//     ___     ___     __                  __                ___     ___
//    |   |   |   |   |  \  |\/|  /\      |__) |  |  |\/|   |   |   |   |
//    |   |   |   |   |__/  |  | /~~\ ___ |    |/\|  |  |   |   |   |   |
//  __|   |___|   |_________________________________________|   |___|   |__
//
// Copyright (c) 2020 Benjamin Spencer
// ============================================================================
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
// ============================================================================
//
// Acknowledgements:
//  - Chris Hager's RPIO
//  - Richard Hirst's ServoBlaster

#ifndef DMA_PWM_H
#define DMA_PWM_H

// Include C POSIX libraries:
#include <stdint.h> // C Standard integer types
#include <stdlib.h> // C Standard library

#define DEFAULT_PULSE_WIDTH 5   // Default PWM pulse width in us
#define DEFAULT_PAGES       16  // Number of pages for each control
                                // block sequence

#define MOTOR_PULSE_WIDTH 0.4   // Motor PWM pulse width in us
#define SERVO_PULSE_WIDTH 50    // Servo PWM pulse width in us
#define LED_PULSE_WIDTH   5000  // LED PWM pulse width in us

// Error numbers:
#define ECHNLREQ    1  // At least one channel has been requested
#define EINVPW      2  // Invalid pulse width
#define ENOFREECHNL 3  // No free DMA channels available to be requested
#define EINVCHNL    4  // Invalid or non-requested channel
#define EINVDUTY    5  // Invalid duty cycle
#define EINVGPIO    6  // Invalid GPIO pin
#define EFREQNOTMET 7  // Desired frequency cannot be met
#define EPWMNOTSET  8  // PWM signal on requested channel has not been set
#define ENOPIVER    9  // Could not get PI board revision
#define EMAPFAIL    10 // Peripheral memory mapping failed
#define ESIGHDNFAIL 11 // Signal handler failed to setup 

// Structure definitions:
struct reg_pwm {
    uint32_t pwm_ctl_ctl;    // Control
    uint32_t pwm_ctl_sta;    // Status
    uint32_t pwm_ctl_dmac;   // DMA configuration
    uint32_t pwm_clk_pwmctl; // Control
    uint32_t pwm_clk_pwmdiv; // Clock divisor
    uint32_t dma_cs;         // Control & status
    uint32_t dma_debug;     // Debug
};

// Set memory usage and pulse width
float config_pwm(int pages, float pulse_width);

// Request an available DMA channel to use for PWM:
int request_pwm();

// Setup a PWM signal for a requested channel
int set_pwm(int channel, int *gpio, size_t num_gpio, \
    float freq, float duty_cycle);

// Enable PWM output for a requested channel
int enable_pwm(int channel);

// Get channel PWM signal duty cycle:
float get_duty_cycle_pwm(int channel);

// Get channel PWM signal duty cycle frequency
float get_freq_pwm(int channel);

// Get PWM pulse width:
float get_pulse_width();

// Disable PWM output for a requested channel
int disable_pwm(int channel);

// Clean-up
int free_pwm(int channel);

// Get register status for debugging:
struct reg_pwm get_reg_pwm(int channel);

#endif // !DMA_PWM_H