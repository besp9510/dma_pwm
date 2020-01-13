// Macros
#define DEFAULT_PULSE_WIDTH 5   // Default PWM pulse width in us
#define DEFAULT_PAGES       16  // Number of pages for each control
                                // block sequence

#define MOTOR_PULSE_WIDTH 0.1   // Motor PWM pulse width in us
#define SERVO_PULSE_WIDTH 100   // Servo PWM pulse width in us
#define LED_PULSE_WIDTH   10000 // LED PWM pulse width in us

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