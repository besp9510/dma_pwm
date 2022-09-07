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
#include <stdint.h> // C Standard integer types

// Include C POSIX libraries:
#include <sys/mman.h> // Memory management library
#include <unistd.h>   // Symbolic constants and types library

// Map peripheral physical address to virtual address
volatile uint32_t* map_peripheral__(uint32_t base_addr) {
    // Definitions:
    int fd;          // File descriptor
    void *virt_addr; // Pointer to virtual address

    // Open /dev/mem for reading and writing
    fd = open("/dev/mem", O_RDWR | O_SYNC);

    // Check success:
    if (fd < 0) {
        // Output message:
        printf("Could not open /dev/mem." \
            " Make sure run as root or with root privileges\n");

        // Exit:
        return NULL;
    }

    // Memory map GPIOs into virtual memory:
    virt_addr = mmap(
        NULL,                   // Any adddress in our space will do
        getpagesize(),          // Map length
        PROT_READ | PROT_WRITE, // Enable reading & writting to mapped memory
        MAP_SHARED,             // Shared with other processes
        fd,                     // File to map
        base_addr               // Offset to peripheral
   );

    // Close /dev/mem:
    close(fd);

    // Check success:
    if (virt_addr == MAP_FAILED) {
        // Exit with error:
        return NULL;
    }

    // Return type casted pointer to unsigned int:
    return (volatile uint32_t*) virt_addr;
}