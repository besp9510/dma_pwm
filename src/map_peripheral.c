// Include C standard libraries:
#include <stdio.h>  // C Standard I/O libary
#include <stdlib.h> // C Standard library
#include <fcntl.h>  // C Standard file control library
#include <stdint.h> // C Standard integer types

// Include C POSIX libraries:
#include <sys/mman.h> // Memory management library
#include <unistd.h>   // Symbolic constants and types library

// Map peripheral physical address to virtual address
volatile uintptr_t* map_peripheral__(uintptr_t base_addr) {
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
    return (volatile uintptr_t*) virt_addr;
}