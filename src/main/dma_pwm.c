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

// Include C standard libraries:
#include <stdio.h>  // C Standard I/O libary
#include <stdlib.h> // C Standard library
#include <fcntl.h>  // C Standard file control library
#include <time.h>   // C Standard get and manipulate time library
#include <string.h> // C Standard string manipulation libary
#include <stdint.h> // C Standard integer types
#include <stddef.h> // C Standard type and macro definitions
#include <signal.h> // C Standard signal processing
#include <errno.h>  // C Standard for error conditions

// Include C POSIX libraries:
#include <sys/mman.h> // Memory management library
#include <unistd.h>   // Symbolic constants and types library

// Include header files:
#include "dma_pwm.h"        // PWM via DMA
#include "gpio.h"           // GPIO macro functions
#include "uncached_mem.h"   // Allocate uncached memory and mapping
                            // (via mailbox) functions
#include "get_pi_version.h" // Get PI board revision
#include "map_peripheral.h" // Map peripherals into memory

// Check if debug logs are enabled:
#ifndef DEBUG
    // Set to false:
    #define DEBUG 0
#endif

// Rounding functions:
#define ROUND(n) (((n) - (int)(n) < 0.5) ? (int)(n) : (int)((n) + 1))
#define CEILING(n) ((int)(n) + 1)

// Base peripheral addresses:
#define BCM2835_PERI_BASE_PHYS_ADDR 0x20000000
#define BCM2835_PERI_BASE_BUS_ADDR  0x7E000000

#define BCM2837_PERI_BASE_PHYS_ADDR 0x3F000000
#define BCM2837_PERI_BASE_BUS_ADDR  0x7E000000

#define BCM2711_PERI_BASE_PHYS_ADDR 0xFE000000
#define BCM2711_PERI_BASE_BUS_ADDR  0x7E000000

// PWM clock manager register offset:
#define PWM_CLK 0xA0

// Register Masks

// PWM clock manager:
#define CM_ENAB (1 << 4)     // Enable the clock generator
#define CM_BASE (0x5A << 24) // Password = 0x5a

// PWM controller:
#define PWM_DMA_ENB      (1 << 31) // DMA enabled
#define PWM_CLRF         (1 << 6)  // Clear FIFO buffer
#define PWM_USEF         (1 << 5)  // FIFO used for transmission
#define PWM_EN1          (1 << 0)  // Channel 1 enabled
#define PWM_DREQ_THRESH  (15 << 0) // Threshold for data required signal
#define PWM_PANIC_THRESH (15 << 8) // Threshold for panic signal

// DMA controller:
#define DMA_NO_WIDE_BURSTS (1 << 26) // Don't do writes as 2 beat bursts
#define DMA_WAIT_RESP      (1 << 3)  // Wait for response for each write
#define DMA_DREQ           (1 << 6)  // Write when data is required
#define DMA_PER_MAP(p)     (p << 16) // Peripheral number whose ready signal
                                     // shall be used to control the rate of
                                     // transfer (5 = PWM)
#define DMA_ACTIVE         (1 << 0)  // Enable DMA
#define DMA_RESET          (1 << 31) // Reset the DMA
#define DMA_ABORT          (1 << 30) // Abort current CB
#define DMA_INT            (1 << 2)  // Clear interrupt
#define DMA_END            (1 << 1)  // Clear transfer complete
#define DMA_WAIT           (1 << 28) // Wait for outstanding writes
#define DMA_PANIC_PRIO(p)  \
    (((p) & 0xF) << 20)              // Set panic bus priority transactions
#define DMA_PRIO(p)        \
    (((p) & 0xF) << 16)              // Set norm bus priority transactions

// Defaults
#define DEFAULT_CLOCK_SOURCE 6     // PLLD
#define DEFAULT_CLOCK_FREQ   500e6 // Source frequency
#define DEFAULT_CLOCK_DIV    50    // Clock integer divisor

#define DEFAULT_PWM_RNG 100 // Period of length

// Constants
#define NUM_DMA_CHANNELS 7 // Number of DMA channels

// Structure definitions

// DMA controller register map:
struct dma_reg_map {
    uint32_t cs;        // Control & status
    uint32_t conblk_ad; // Control block address
    uint32_t ti;        // Transfer information
    uint32_t source_ad; // Source address
    uint32_t dest_ad;   // Destination address
    uint32_t txfr_len;  // Transfer length
    uint32_t stride;    // 2D stride
    uint32_t nextconbk; // NExt CB address
    uint32_t debug;     // Debug
};

// PWM controller register map:
struct pwm_ctl_reg_map {
    uint32_t ctl;  // Control
    uint32_t sta;  // Status
    uint32_t dmac; // DMA configuration
    uint32_t pad1; // Padding
    uint32_t rng1; // Channel 1 range
    uint32_t dat1; // Channel 1 data
    uint32_t fif1; // FIFO input
    uint32_t pad2; // Padding
    uint32_t rng2; // Channel 2 range
    uint32_t dat2; // Channel 2 data
};

// PWM clock register map:
struct pwm_clk_reg_map {
    uint32_t pwmctl; // Control
    uint32_t pwmdiv; // Clock divisor
};

// PWM DMA channel:
struct channel {
    // Memory addresses:
    struct uncached_mem *cb_base[2];    // CB base uncached memory struct
    struct uncached_mem *set_mask[2];   // GPIO set mask uncached memory struct
    struct uncached_mem *clear_mask[2]; // GPIO clear mask uncached memory struct

    uint32_t cb_base_bus_addr[2];    // Control block seq. phys base address
    uint32_t set_mask_bus_addr[2];   // GPIO set mask physical address
    uint32_t clear_mask_bus_addr[2]; // GPIO clear mask physical address

    volatile struct dma_reg_map *dma_reg; // DMA register map for the channel

    // Inputs:
    float freq_des;   // Desired frequency
    float pwm_d_des;  // Desired PWM duty cycle

    // Attributes:
    float pwm_d_res;  // PWM duty cycle resolution
    float freq_act;   // Actual frequency
    float pwm_d_act;  // Actual PWM duty cycle

    unsigned t_sub_us; // Sub cycle period

    size_t cb_seq_num; // Number of control blocks in sequence
    size_t cb_clr_num; // Number of "wait" control blocks during GPIO clear
    size_t cb_set_num; // Number of "wait" control blocks during GPIO set

    // Flags:
    uint8_t enabled;         // Channel enabled
    uint8_t selected_cb_buf; // Selected CB buffer (0 or 1)
    uint8_t seq_built;       // PWM signal set
};

// DMA controller control block:
struct dma_cb {
    uint32_t info;   // TI
    uint32_t src;    // SOURCE_AD
    uint32_t dst;    // DEST_AD
    uint32_t length; // TXFR_LEN
    uint32_t stride; // 2D stride mode
    uint32_t next;   // NEXTCONBK
    uint32_t res[2]; // Reserved
};

// Global variables
static int gpio_base_phys_addr;    // GPIO base physical address
static int dma_ctl_base_phys_addr; // DMA base physical address
static int pwm_ctl_base_phys_addr; // PWM base physical address
static int pwm_clk_base_phys_addr; // PWM clock base physical address

static int gpset0_bus_addr;  // GPIO set bus address
static int gpclr0_bus_addr;  // GPIO clear bus address
static int pwmfif1_bus_addr; // PWM FIF1 bus address

static volatile uint32_t *gpio_base_virt_addr;    // GPIO base virt. ad.
static volatile uint32_t *dma_ctl_base_virt_addr; // DMA contoller virt. ad.
static volatile uint32_t *pwm_ctl_base_virt_addr; // PWM Controller virt ad.
static volatile uint32_t *pwm_clk_base_virt_addr; // PWM Clock Manager virt.
                                                   // address

static int pi_version; // Raspberry PI board version

static unsigned int clock_div = DEFAULT_CLOCK_DIV; // Clock divisor
static unsigned int pwm_rng = DEFAULT_PWM_RNG;     // Period of length

static int allocated_pages = DEFAULT_PAGES; // Allocated uncached memory
                                            // pages for CB sequence

static float pulse_width_us; // PWM signal pulse width

// Tested on Raspberry Pi 3b+ running Linux raspberrypi 5.10.17-v7+ #1403
// No interruptions observed over 40 minute continuous output
// Do not use channels: 0, 1, 2, 3, 5, 6, 7
// Use channels: 8, 9, 10, 11, 12, 13, 14
static int valid_dma_channels[NUM_DMA_CHANNELS] = \
    {10, 8, 9, 11, 12, 13, 14}; // DMA channels to use

static int dma_channels_status[NUM_DMA_CHANNELS] = \
    {1, 1, 1, 1, 1, 1, 1}; // 1 = Availabe/free

static struct channel dma_channels[NUM_DMA_CHANNELS]; // Channel structure for
                                                      // each DMA channel

static volatile struct pwm_ctl_reg_map *pwm_ctl_reg; // PWM controller reg map
static volatile struct pwm_clk_reg_map *pwm_clk_reg; // Clock manager reg map

static int init_state = 0; // Initialized?

// DMA delay per data sheet:
static struct timespec delay = {
    .tv_sec = 0,
    .tv_nsec = 10e3 // 10 us
};

// Procees termination signal handler
static void signal_handler(int sig, siginfo_t* sig_info, void* context) {
    // Definitions:
    int i;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Signal %d received; aborting!\n", sig);
    }

    // Terminate any used DMA channels and free allocated memory:
    for (i = 0; i < NUM_DMA_CHANNELS; i++) {
        // Diabled and free:
        free_pwm(i);
    }
}

// Setup signal handler to catch process terminating signals
// (Required to terminate DMA and free allocated uncached memory)
static int setup_signal_handler() {
    // Definitions:
    int i;
    int signals[4] = {SIGHUP, SIGQUIT, SIGINT, SIGTERM}; // Signals to catch

    struct sigaction sig; // Signal action structure

    // Assign handler to sig:
    sig.sa_sigaction = &signal_handler;
    sig.sa_flags = SA_SIGINFO;

    // Assign signal handler to each termination signal:
    for (i = 0; i < (sizeof(signals) / sizeof(int)); i++) {
        // Register handler with sigaction for signal type:
        if (sigaction(signals[i], &sig, NULL) < 0) {
            // Debug logs:
            if (DEBUG) {
                // Logs:
                printf("ERROR: Signal %d not registered with signal" \
                    " handler\n", signals[i]);
                printf("ERROR: setup_signal_handler() returned" \
                    " with %d\n", -ESIGHDNFAIL);
            }

            // Exit with error:
            return -ESIGHDNFAIL;
        }

        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("Signal %d registered with signal handler\n", signals[i]);
        }
    }

    // Exit with success:
    return 0;
}

// Check channel
static int check_channel(int channel) {
    // Abort if channel does not make sense:
    if ((channel < 0) || (channel >= NUM_DMA_CHANNELS)) {
        // Debug logs:
        if (DEBUG) {
            // Log message:
            printf("ERROR: channel %d is nonsensical\n", channel);
        }

        // Exit with error:
        return -EINVCHNL;
    }

    // Abort if channel was not requested:
    if (dma_channels_status[channel]) {
        // Debug logs:
        if (DEBUG) {
            // Log message:
            printf("ERROR: channel %d is not requested\n", channel);
        }

        // Exit with error:
        return -EINVCHNL;
    }

    // Exit with success:
    return 0;
}

// Set memory usage and pulse width
float config_pwm(int pages, float pulse_width) {
    // Definitions:
    float pulse_width_us_temp;
    unsigned int clock_div_temp;
    unsigned int pwm_rng_temp;

    int i;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Configuring dma_pwm.c\n");
    }

    // Abort if any channel is requested:
    for (i = 0; i < NUM_DMA_CHANNELS; i++) {
        // Exit with error if requested:
        if (!(dma_channels_status[i])) {
            // Debug logs:
            if (DEBUG) {
                // Logs:
                printf("ERROR: channel %d has been requested\n", i);
                printf("ERROR: config_pwm() returned with %d\n", -ECHNLREQ);
            }

            // Exit with error:
            return -ECHNLREQ;
        }
    }

    // Set to current:
    pwm_rng_temp = pwm_rng;

    // Update:
    allocated_pages = pages;

    // Check is desired pulse width is out of bounds:
    if ((pulse_width > 35175782146) || (pulse_width < 0.4)) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: pulse width %0.3f out of bounds\n", pulse_width);
            printf("ERROR: config_pwm() returned with %d\n", -EINVPW);
        }
        // Exit with error:
        return -EINVPW;
    }

    // Calculate new clock divisor if range remained constant:
    clock_div_temp = ((pulse_width / 1000000.0) / pwm_rng) * \
        DEFAULT_CLOCK_FREQ;

    // Check if it's out of bounds:
    if ((clock_div_temp < 1) || (clock_div_temp > 4095)) {
        // Round divisor to appropriate bound:
        clock_div_temp = (clock_div_temp < 1) ? 1 : 4095;

        // Calculate pwm range:
        pwm_rng_temp = (pulse_width / 1000000.0) * \
            ((float)DEFAULT_CLOCK_FREQ / clock_div_temp);

        // Check pwm range is within bounds (redundant check):
        if (pwm_rng_temp < 1) {
            // Debug logs:
            if (DEBUG) {
                // Logs:
                printf("ERROR: pulse width %0.3f cannot be computed\n", \
                    pulse_width);
                printf("Clock divisor = %d, PWM RNG1 = %d\n", clock_div_temp, \
                    pwm_rng_temp);
                printf("ERROR: config_pwm() returned with %d\n", -EINVPW);
            }

            // Exit with error:
            return -EINVPW;
        }
    }

    // Calculate achieved pulse width:
    pulse_width_us_temp = \
        (pwm_rng_temp / (DEFAULT_CLOCK_FREQ / clock_div_temp)) * 1e6;

    // Update:
    clock_div = clock_div_temp;
    pwm_rng = pwm_rng_temp;
    pulse_width_us = pulse_width_us_temp;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Setting pulse width to %0.3f us\n", pulse_width_us);
        printf("Setting number of allocated pages to %d\n", allocated_pages);
        printf("Configured dma_pwm.c\n");
    }

    // Exit with achieved pulse width:
    return 0;
}

// Initialize
static int init_pwm() {
    // Definitions:
    int bcm_peri_base_phys_addr;
    int bcm_peri_base_bus_addr;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("DEBUG logs enabled for dma_pwm.c!\n");
        printf("Initializing dma_pwm.c\n");
    }

    // Setup termination signal handler:
    if (setup_signal_handler() != 0) {
        // Exit with error:
        return -ESIGHDNFAIL;
    }

    // Get PI version:
    pi_version = get_pi_version__();

    // Set BCM base addresses according to PI version:
    if ((pi_version == 0) || (pi_version == 1)) {
        // Set BCM base addresses:
        bcm_peri_base_phys_addr = BCM2835_PERI_BASE_PHYS_ADDR;
        bcm_peri_base_bus_addr = BCM2835_PERI_BASE_BUS_ADDR;
    } else if ((pi_version == 2) || (pi_version == 3)) {
        // Set BCM base addresses:
        bcm_peri_base_phys_addr = BCM2837_PERI_BASE_PHYS_ADDR;
        bcm_peri_base_bus_addr = BCM2837_PERI_BASE_BUS_ADDR;
    } else if (pi_version == 4) {
        // Set BCM base addresses:
        bcm_peri_base_phys_addr = BCM2711_PERI_BASE_PHYS_ADDR;
        bcm_peri_base_bus_addr = BCM2711_PERI_BASE_BUS_ADDR;
    // Did not find PI version:
    } else {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: get_pi_version() could not get PI board" \
                " version\n");
        }

        // Exit with error:
        return -ENOPIVER;
    }

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Setting PI board version as %d\n", pi_version);
        printf("BCM peripheral base physical address = 0x%08X\n", \
            bcm_peri_base_phys_addr);
        printf("BCM peripheral base bus address = 0x%08X\n", \
            bcm_peri_base_bus_addr);
    }

    // Set peripheral addresses:
    gpio_base_phys_addr = \
        (bcm_peri_base_phys_addr + 0x200000); // GPIOs
    dma_ctl_base_phys_addr = \
        (bcm_peri_base_phys_addr + 0x007000); // DMA controller
    pwm_ctl_base_phys_addr = \
        (bcm_peri_base_phys_addr + 0x20C000); // PWM Controller
    pwm_clk_base_phys_addr = \
        (bcm_peri_base_phys_addr + 0x101000); // PWM Clock Manager

    gpset0_bus_addr = (bcm_peri_base_bus_addr + 0x20001C); // GPIO Set
    gpclr0_bus_addr = (bcm_peri_base_bus_addr + 0x200028); // GPIO Clear
    pwmfif1_bus_addr = (bcm_peri_base_bus_addr + 0x20C018); // PWM FIFO buffer

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("GPSET0 bus address = 0x%08X\n", gpset0_bus_addr);
        printf("GPCLR0 bus address = 0x%08X\n", gpclr0_bus_addr);
        printf("PWMFIF1 bus address = 0x%08X\n", gpclr0_bus_addr);
    }

    // Calculate PWM pulse width:
    pulse_width_us = 1e6 * \
        (pwm_rng / (DEFAULT_CLOCK_FREQ / clock_div));

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Setting pulse width to %0.3f us\n", pulse_width_us);
    }

    // Map peripherals into virtual memory:
    gpio_base_virt_addr = map_peripheral__(gpio_base_phys_addr);
    dma_ctl_base_virt_addr = map_peripheral__(dma_ctl_base_phys_addr);
    pwm_ctl_base_virt_addr = map_peripheral__(pwm_ctl_base_phys_addr);
    pwm_clk_base_virt_addr = map_peripheral__(pwm_clk_base_phys_addr);

    // Abort if mapped incorrectly:
    if ((gpio_base_virt_addr == NULL) || (dma_ctl_base_virt_addr == NULL) || \
       (pwm_ctl_base_virt_addr == NULL) || (pwm_clk_base_virt_addr == NULL)) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: map_peripheral() returned a NULL address\n");
            printf("GPIO base virtual address = %p\n", gpio_base_virt_addr);
            printf("DMA CTL base virtual address = %p\n", \
                dma_ctl_base_virt_addr);
            printf("PWM CTL base virtual address = %p\n", \
                pwm_ctl_base_virt_addr);
            printf("PWM CLK base virtual address  = %p\n", \
                pwm_clk_base_virt_addr);
            printf("Most likely did not run as root\n");
            printf("ERROR: init_pwm() returned with %d\n", -EMAPFAIL);
        }

        // Exit with error:
        return -EMAPFAIL;
    }

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Mapped peripherals into virtual memory:\n");
        printf("GPIO base virtual address = %p\n", gpio_base_virt_addr);
            printf("DMA CTL base virtual address = %p\n", \
                dma_ctl_base_virt_addr);
            printf("PWM CTL base virtual address = %p\n", \
                pwm_ctl_base_virt_addr);
            printf("PWM CLK base virtual address  = %p\n", \
                pwm_clk_base_virt_addr);
    }

    // Set mapped peripheral address to register maps:
    pwm_ctl_reg = (struct pwm_ctl_reg_map*)pwm_ctl_base_virt_addr;
    pwm_clk_reg = (struct pwm_clk_reg_map*)\
        ((char*)pwm_clk_base_virt_addr + PWM_CLK);

    // Reset PWM controller:
    pwm_ctl_reg->ctl = 0;

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Set clock source:
    pwm_clk_reg->pwmctl = (CM_BASE | (DEFAULT_CLOCK_SOURCE << 0));

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Set clock integer divisor:
    pwm_clk_reg->pwmdiv = (CM_BASE | (clock_div << 12));

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Enable clock:
    pwm_clk_reg->pwmctl = (CM_BASE | (DEFAULT_CLOCK_SOURCE << 0) | CM_ENAB);

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Set period of length:
    pwm_ctl_reg->rng1 = pwm_rng;

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Enable DMA and set threshold
    pwm_ctl_reg->dmac = (PWM_DMA_ENB | PWM_DREQ_THRESH | PWM_PANIC_THRESH);

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Clear FIFO buffer:
    pwm_ctl_reg->ctl = PWM_CLRF;

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Use FIFO buffer and enable channel:
    pwm_ctl_reg->ctl = (PWM_USEF | PWM_EN1);

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("PWM CTL and CM initialized:\n");
        printf("PWM CM Register: PWMDIV = 0x%08X\n", pwm_clk_reg->pwmdiv);
        printf("PWM CM Register: PWMCTL = 0x%08X\n", pwm_clk_reg->pwmctl);
        printf("PWM CTL Register: CTL = 0x%08X\n", pwm_ctl_reg->ctl);
        printf("PWM CTL Register: DMAC = 0x%08X\n", pwm_ctl_reg->dmac);
        printf("PWM CTL Register: RNG1 = 0x%08X\n", pwm_ctl_reg->rng1);
        printf("Initialized dma_pwm.c\n");
    }

    // Exit with success:
    return 0;
}

// Initialize memory and hardware for PWM:
// - Set up PWM clock mananger
// - Set up PWM controller
// - Set up all DMA channels
// - Allocate page aligned memory for CB
static int init_channel(int channel) {
    // Definitions:
    int i;

    size_t page_size; // Page size

    // Get page size:
    page_size = getpagesize();

    // Set flags:
    dma_channels[channel].enabled = 0;
    dma_channels[channel].selected_cb_buf = 1;
    dma_channels[channel].seq_built = 0;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Initializing channel %d\n", channel);
        printf("Setting page size to %d bytes\n", page_size);
        printf("Setting channel %d selected CB buffer to %d\n", channel, \
            dma_channels[channel].selected_cb_buf);
    }

    // Initialize for each buffer:
    for (i = 0; i < 2; i++) {
        // Allocate memory for uncached mem structs:
        dma_channels[channel].cb_base[i] = \
            malloc(sizeof(struct uncached_mem));
        dma_channels[channel].set_mask[i] = \
            malloc(sizeof(struct uncached_mem));
        dma_channels[channel].clear_mask[i] = \
            malloc(sizeof(struct uncached_mem));

        // Set uncached memory struct size and alignment:
        dma_channels[channel].cb_base[i]->size = \
            page_size * allocated_pages;
        dma_channels[channel].cb_base[i]->alignment = page_size;

        dma_channels[channel].set_mask[i]->size = sizeof(uint32_t);
        dma_channels[channel].set_mask[i]->alignment = sizeof(uint32_t);

        dma_channels[channel].clear_mask[i]->size = sizeof(uint32_t);
        dma_channels[channel].clear_mask[i]->alignment = sizeof(uint32_t);

        // Allocate page aligned uncached memory via mailbox:
        dma_channels[channel].cb_base[i] = \
            uncached_malloc__(dma_channels[channel].cb_base[i]);
        dma_channels[channel].set_mask[i] = \
            uncached_malloc__(dma_channels[channel].set_mask[i]);
        dma_channels[channel].clear_mask[i] = \
            uncached_malloc__(dma_channels[channel].clear_mask[i]);

        // Bus addresses for DMA transfer:
        dma_channels[channel].cb_base_bus_addr[i] = \
            dma_channels[channel].cb_base[i]->bus_addr;
        dma_channels[channel].set_mask_bus_addr[i] = \
            dma_channels[channel].set_mask[i]->bus_addr;
        dma_channels[channel].clear_mask_bus_addr[i] = \
            dma_channels[channel].clear_mask[i]->bus_addr;
    }

    // Map DMA register map:
    dma_channels[channel].dma_reg = \
        (struct dma_reg_map*)((char *)dma_ctl_base_virt_addr + \
        0x100 * valid_dma_channels[channel]);

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Channel %d initialized with %d bytes allocated (x2)\n", \
            channel, (page_size * allocated_pages));
    }

    // Exit with success:
    return 0;
}

// Request an available DMA channel to use for PWM:
int request_pwm() {
    // Definitions:
    int i;
    int ret; // Function return value

    int channel; // Available DMA channel

    // Initialize if not yet initialized:
    if (!(init_state)) {
        // Initialize:
        if ((ret = init_pwm()) < 0) {
            // Debug logs:
            if (DEBUG) {
                // Logs:
                printf("ERROR: Could not initialize dma_pwm.c\n");
            }

            // Exit with error:
            return ret;
        }

        // Set status to true:
        init_state = 1;
    }

    // Find available channel
    for (i = 0; i < NUM_DMA_CHANNELS; i++) {
        // Check if channel is available:
        if (dma_channels_status[i]) {
            // Set channel as index:
            channel = i;

            // Update channel status:
            dma_channels_status[i] = 0;

            // Exit:
            break;
        // Not available:
        } else {
            // Check if at end of list:
            if (i == NUM_DMA_CHANNELS) {
                // Debug logs:
                if (DEBUG) {
                    // Logs:
                    printf("ERROR: No free channels to request\n");
                    printf("ERROR: request_pwm() returned with %d\n", \
                        -ENOFREECHNL);
                }

                // Exit with error:
                return -ENOFREECHNL;
            }
        }
    }

    // Initialize channel:
    init_channel(channel);

    // Return available channel:
    return channel;
}

// Build control block sequence for a DMA channel:
static void build_cb_seq(int channel) {
    // Definitions:
    int i;

    int cb_buf; // Which CB buffer to use

    // Get which CB buffer to use:
    cb_buf = dma_channels[channel].selected_cb_buf;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Building CB sequence for channel %d on buffer %d \n", \
            channel, cb_buf);
    }

    // Assign control block sequence to channel's cb virtual base:
    // (virtual base begins at a page-aligned addresses located in
    // uncached memory)
    struct dma_cb *dma_cb_seq = \
        (struct dma_cb*)dma_channels[channel].cb_base[cb_buf]->virt_addr;

    // Build CB sequence:
    for (i = 0; i < dma_channels[channel].cb_seq_num; i++) {
        // Set or clear GPIOs depending on duty cycle for first CB:
        if (i == 0) {
            // Build control block
            dma_cb_seq->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP;

            // Set GPIOs if duty cycle is not 0:
            if ((int)dma_channels[channel].pwm_d_des != 0) {
                dma_cb_seq->src = \
                    dma_channels[channel].set_mask_bus_addr[cb_buf];
                dma_cb_seq->dst = gpset0_bus_addr;
            } else {
                dma_cb_seq->src = \
                    dma_channels[channel].clear_mask_bus_addr[cb_buf];
                dma_cb_seq->dst = gpclr0_bus_addr;
            }
            dma_cb_seq->length = 4;
            dma_cb_seq->stride = 0;
            dma_cb_seq->next = uncached_virt_to_bus_addr__( \
                dma_channels[channel].cb_base[cb_buf], (dma_cb_seq + 1));
        // Clear GPIOs for non-trivial duty cycles (no need to clear in
        // those cases once "wait" CB while GPIO is set expends):
        } else if ((i == (dma_channels[channel].cb_set_num + 1)) && \
        ((int)dma_channels[channel].pwm_d_des % 100 != 0)) {
            // Build control block
            dma_cb_seq->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP;
            dma_cb_seq->src = \
                dma_channels[channel].clear_mask_bus_addr[cb_buf];
            dma_cb_seq->dst = gpclr0_bus_addr;
            dma_cb_seq->length = 4;
            dma_cb_seq->stride = 0;
            dma_cb_seq->next = uncached_virt_to_bus_addr__( \
                dma_channels[channel].cb_base[cb_buf], (dma_cb_seq + 1));
        // Write data to PWM controller to wait if no need to clear or
        // set GPIOs:
        } else {
            // Build control block
            dma_cb_seq->info = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP | \
                DMA_DREQ | DMA_PER_MAP(5);
            dma_cb_seq->src = 0xABCDEF; // Random data
            dma_cb_seq->dst = pwmfif1_bus_addr;
            dma_cb_seq->length = 4;
            dma_cb_seq->stride = 0;
            // Link to beginning of the sequence if last iteration:
            if (i == (dma_channels[channel].cb_seq_num - 1)) {
                // Link to beginning:
                dma_cb_seq->next = \
                    dma_channels[channel].cb_base_bus_addr[cb_buf];
            // Continue:
            } else {
                // Link to next CB:
                dma_cb_seq->next = uncached_virt_to_bus_addr__( \
                    dma_channels[channel].cb_base[cb_buf], (dma_cb_seq + 1));
            }
        }

        // Increment control block:
        dma_cb_seq++;
    }

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Built CB sequence for channel %d on buffer %d \n", \
            channel, cb_buf);
    }
}

// Setup a PWM signal for a requested channel:
int set_pwm(int channel, int* gpio, size_t num_gpio, \
    float freq, float duty_cycle) {
    // Definitions:
    int i;

    int ret; // Function return value

    unsigned t_sub_us; // PWM sub cycle period
    size_t cb_seq_num; // CB sequence length
    size_t pages_req;  // Number of required pages for the CB sequence
    float freq_act;    // Actual PWM frequency achieved
    float pwm_d_res;   // PWM duty cycle resolution
    float pwm_d_act;   // Actual PWM duty cycle achieved 
    size_t cb_set_num; // Number of "wait" CBs while GPIO set
    size_t cb_clr_num; // Number of "wait" CBs while GPIO clear

    uint32_t set_mask = 0;   // GPIO set mask
    uint32_t clear_mask = 0; // GPIO clear mask

    int cb_buf; // Which CB buffer to use

    // Debug logs:
    if (DEBUG) {
        // Log message:
        printf("PWM signal to be set on channel %d\n", channel);
    }

    // Check channel:
    if ((ret = check_channel(channel)) < 0) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: set_pwm() returned %d\n", ret);
        }

        // Exit with error:
        return ret;
    }

    // Abort if duty cycle is not within bounds:
    if ((duty_cycle < 0) || (duty_cycle > 100)) {
        // Debug logs:
        if (DEBUG) {
            // Log message:
            printf("ERROR: duty cycle %0.3f is out of bounds\n", duty_cycle);
            printf("ERROR: set_pwm() returned %d\n", -EINVDUTY);
        }

        // Exit with error:
        return -EINVDUTY;
    }

    // Abort if input GPIOs do not make sense:
    for (i = 0; i < num_gpio; i++) {
        if ((gpio[i] < 0) || (gpio[i] > 31)) {
            // Debug logs:
            if (DEBUG) {
                // Log message:
                printf("ERROR: GPIO %d is not valid\n", gpio[i]);
                printf("ERROR: set_pwm() returned %d\n", -EINVGPIO);
            }

            // Exit with error:
            return -EINVGPIO;
        }
    }

    // Determine sub cycle period:
    t_sub_us = 1e6 * (1.0 / freq);

    // Determine number of CBs required:
    cb_seq_num = (t_sub_us / pulse_width_us) / 2;

    // Determine the number of required pages for the CB seq:
    pages_req = CEILING((cb_seq_num / getpagesize()));

    // Abort if number of CBs is 0 (desired frequency cannot be met):
    if (cb_seq_num == 0) {
        // Exit with error:
        return -EFREQNOTMET;
    }

    // Abort if number of required pages for the CB sequence is greater
    // than what's allocated:
    if (pages_req > allocated_pages) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: CB sequence pages required > allocated pages\n");
            printf("ERROR: CB sequence pages required > allocated pages\n");
        }

        // Exit with error:
        return -ENOMEM;
    }

    // Determine achieved frequency:
    freq_act = (1.0 / (cb_seq_num * pulse_width_us * 1e-6)) / 2;

    // Determine PWM resolution:
    pwm_d_res = 100.0 * (1 - (cb_seq_num - 1) / (float)cb_seq_num);

    // For duty cycles of 0 and 100, desired can always be achieved:
    if ((int)duty_cycle % 100 == 0) {
        // Set actual to desired duty cycle:
        pwm_d_act = duty_cycle;
    // For all other duty cycles, must round desired to what can be
    // achieved:
    } else {
        // Round desired PWM to nearest multiple of its resolution:
        pwm_d_act = ROUND(duty_cycle / pwm_d_res) * pwm_d_res;
    }

    // Determine number of set and clear CBs:
    cb_set_num = (t_sub_us / pulse_width_us) * (pwm_d_act / 100) / 2;
    cb_clr_num = abs(cb_seq_num - cb_set_num);

    // Add additional number of CBs for GPIO set and clear:
    cb_seq_num += ((int)duty_cycle % 100 == 0) ? 1 : 2;

    // Select alternate buffer to use (it's not active):
    cb_buf = (dma_channels[channel].selected_cb_buf ? 0 : 1);

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Selecting CB buffer %d for channel %d \n", \
            cb_buf, channel);
    }

    // Set input GPIOs to output and define set and clear mask:
    for (i = 0; i < num_gpio; i++) {
        // Set pin to output if not already:
        GPIO_OUT(gpio_base_virt_addr, gpio[i]);

        // Append to mask:
        set_mask |= (1 << gpio[i]);
        clear_mask |= (1 << gpio[i]);
    }

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Setting GPIO masks\n");
        printf("GPIO set mask = 0x%08X\n", clear_mask);
        printf("GPIO clear mask = 0x%08X\n", set_mask);
    }

    // Update masks:
    *(uint32_t*)dma_channels[channel].clear_mask[cb_buf]->virt_addr = \
        clear_mask;
    *(uint32_t*)dma_channels[channel].set_mask[cb_buf]->virt_addr = set_mask;

    // Update channel structure:
    dma_channels[channel].t_sub_us = t_sub_us;
    dma_channels[channel].freq_des = freq;
    dma_channels[channel].freq_act = freq_act;
    dma_channels[channel].pwm_d_des = duty_cycle;
    dma_channels[channel].pwm_d_act = pwm_d_act;
    dma_channels[channel].cb_seq_num = cb_seq_num;
    dma_channels[channel].cb_set_num = cb_set_num;
    dma_channels[channel].cb_clr_num = cb_clr_num;
    dma_channels[channel].selected_cb_buf = cb_buf;

    // Debug logs:
    if (DEBUG) { 
        printf("Setting PWM signal and CB sequence properties:\n");
        printf("Pulse width = %0.4f us\n", pulse_width_us);
        printf("Subcycle = %d us\n", t_sub_us);
        printf("Actual frequency = %.7f Hz\n", freq_act);
        printf("Duty cycle resolution = %.7f%%\n", pwm_d_res);
        printf("Actual duty cycle = %.7f%%\n", pwm_d_act);
        printf("CB sequence \"set\" number = %d\n", cb_set_num);
        printf("CB sequence \"clear\" number = %d\n", cb_clr_num);
        printf("CB sequence total number = %d\n", cb_seq_num);
    }

    // Build control block sequence for the DMA channel:
    build_cb_seq(channel);

    // Update flags:
    dma_channels[channel].seq_built = 1;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Channel %d PWM signal set\n", channel);
    }

    // Load CB and start DMA if channel is already enabled (update PWM signal):
    if (dma_channels[channel].enabled) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("Updating PWM signal now on channel %d as it's enabled\n",\
                channel);
        }

        // Update PWM signal:
        enable_pwm(channel);
    }

    // Exit:
    return 0;
}

// Enable PWM output for a channel:
int enable_pwm(int channel) {
    // Definitions
    uint8_t cb_buf; // Which CB buffer to use

    int ret; // Function return value

    // Debug logs:
    if (DEBUG) {
        // Log message:
        printf("Channel %d to be enabled\n", channel);
    }

    // Check channel:
    if ((ret = check_channel(channel)) < 0) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: enable_pwm() returned %d\n", ret);
        }

        // Exit with error:
        return ret;
    }

    // Abort if channel does not have a CB sequence built:
    if (!(dma_channels[channel].seq_built)) {
        // Debug logs:
        if (DEBUG) {
            // Log message:
            printf("ERROR: channel %d has no PWM signal set\n", channel);
            printf("ERROR: enable_pwm() returned %d\n", -EPWMNOTSET);
        }

        // Exit with error:
        return -EPWMNOTSET;
    }

    // Get which CB buffer to load:
    cb_buf = dma_channels[channel].selected_cb_buf;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Loading CB sequence from buffer %d \n", cb_buf);
    }

    // Abort current DMA transfer:
    dma_channels[channel].dma_reg->cs |= DMA_ABORT;

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Pause DMA transfer:
    dma_channels[channel].dma_reg->cs &= ~DMA_ACTIVE;

    // Clear transfer complete:
    dma_channels[channel].dma_reg->cs |= DMA_END;

    // Reset channel:
    dma_channels[channel].dma_reg->cs |= DMA_RESET;

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Load first control block:
    dma_channels[channel].dma_reg->conblk_ad = \
        dma_channels[channel].cb_base_bus_addr[cb_buf];

    // Set transaction priority and wait for outstanding writes:
    dma_channels[channel].dma_reg->cs = \
        DMA_PANIC_PRIO(7) | DMA_PRIO(7) | DMA_WAIT;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("DMA Channel %d Register: CONBLK_AD = 0x%08X\n", \
            channel, dma_channels[channel].dma_reg->conblk_ad);
    }

    // Let's go:
    dma_channels[channel].dma_reg->cs |= DMA_ACTIVE;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("DMA Channel %d Register: CS = 0x%08X\n", \
            channel, dma_channels[channel].dma_reg->cs);
    }

    // Update/enforce channel status:
    dma_channels[channel].enabled = 1;

    // Debug logs:
    if (DEBUG) {
        // Logs:
        printf("Channel %d enabled\n", channel);
    }

    // Exit:
    return 0;
}

// Clear all channel GPIOs
static int clear_channel_gpio(int channel) {
    // Definitions:
    int i;

    int bit;           // Bit state
    uint32_t set_mask; // GPIO set mask

    uint8_t cb_buf; // Which CB buffer to use

    // Get which CB buffer to use:
    cb_buf = dma_channels[channel].selected_cb_buf;

    // Get GPIO set mask to know which GPIOs to clear:
    set_mask = *(uint32_t*) \
        dma_channels[channel].set_mask[cb_buf]->virt_addr;

    // Clear GPIOs:
    for (i = 0; i < sizeof(uint32_t) * 8; i++) {
        // Get bit value at the position:
        bit = (set_mask >> i) & 0x01;

        // Clear GPIO if it set:
        if (bit) {
            // Debug logs:
            if (DEBUG) {
                // Logs:
                printf("GPIO BCM pin %d cleared\n", i);
            }

            // Clear pin:
            GPIO_CLEAR(gpio_base_virt_addr, i);
        }
    }

    // Exit with success:
    return 0;
}

// Disable DMA channel
int disable_pwm(int channel) {
    // Definitions:
    int ret; // Function return value
    
    // Debug logs:
    if (DEBUG) {
        // Log message:
        printf("Channel %d to be disabled\n", channel);
    }

    // Check channel:
    if ((ret = check_channel(channel)) < 0) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: disable_pwm() returned %d\n", ret);
        }

        // Exit with error:
        return ret;
    }

    // Abort current DMA transfer:
    dma_channels[channel].dma_reg->cs |= DMA_ABORT;

    // Delay per data sheet:
    nanosleep(&delay, NULL);

    // Pause DMA transfer:
    dma_channels[channel].dma_reg->cs &= ~DMA_ACTIVE;

    // Reset channel:
    dma_channels[channel].dma_reg->cs |= DMA_RESET;

    // Clear GPIOs:
    clear_channel_gpio(channel);

    // Update channel structure:
    dma_channels[channel].enabled = 0;

    // Debug logs:
    if (DEBUG) {
        // Log message:
        printf("Channel %d disabled\n", channel);
    }

    // Exit with success:
    return 0;

}

// Clean-up
// - Halt DMA controller
// - Free allocated memory
int free_pwm(int channel) {
    // Definitions:
    int i;

    int ret; // Function return value

    // Debug logs:
    if (DEBUG) {
        // Log message:
        printf("Channel %d to be freed\n", channel);
    }

    // Check channel:
    if ((ret = check_channel(channel)) < 0) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: free_pwm() returned %d\n", ret);
        }

        // Exit with error:
        return ret;
    }

    // Disable DMA channel (if not already disabled):
    disable_pwm(channel);

    // Free both buffers:
    for (i = 0; i < 2; i++) {
        // Free allocated uncached memory:
        uncached_free__(dma_channels[channel].cb_base[i]);
        uncached_free__(dma_channels[channel].set_mask[i]);
        uncached_free__(dma_channels[channel].clear_mask[i]);

        // Free allocated memory:
        free(dma_channels[channel].cb_base[i]);
        free(dma_channels[channel].set_mask[i]);
        free(dma_channels[channel].clear_mask[i]);
    }

    // Set flags:
    dma_channels[channel].enabled = 0;
    dma_channels[channel].seq_built = 0;

    // Update channel status:
    dma_channels_status[channel] = 1;

    // Debug logs:
    if (DEBUG) {
        // Log message:
        printf("Channel %d freed\n", channel);
    }

    // Return success:
    return 0;
}

// Get channel PWM signal duty cycle:
float get_duty_cycle_pwm(int channel) {
    // Definitions:
    int ret; // Function return value

    // Check channel:
    if ((ret = check_channel(channel)) < 0) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: get_duty_cycle_pwm() returned %d\n", ret);
        }

        // Exit with error:
        return ret;
    }

    // Return duty cycle:
    return dma_channels[channel].pwm_d_act;
}

// Get channel PWM signal duty cycle frequency:
float get_freq_pwm(int channel) {
    // Definitions:
    int ret; // Function return value

    // Check channel:
    if ((ret = check_channel(channel)) < 0) {
        // Debug logs:
        if (DEBUG) {
            // Logs:
            printf("ERROR: get_freq_pwm() returned %d\n", ret);
        }

        // Exit with error:
        return ret;
    }

    // Return frequency:
    return dma_channels[channel].freq_act;
}

// Get PWM pulse width:
float get_pulse_width(void) {
    // Return pulse width:
    return pulse_width_us;
}

// Get register status for debugging:
struct reg_pwm get_reg_pwm(int channel) {
    struct reg_pwm reg = {
        .pwm_ctl_ctl = pwm_ctl_reg->ctl,
        .pwm_ctl_sta = pwm_ctl_reg->sta,
        .pwm_ctl_dmac = pwm_ctl_reg->dmac,
        .pwm_clk_pwmctl = pwm_clk_reg->pwmctl,
        .pwm_clk_pwmdiv = pwm_clk_reg->pwmdiv,
        .dma_cs = dma_channels[channel].dma_reg->cs,
        .dma_debug = dma_channels[channel].dma_reg->debug
    };
    return reg;
}