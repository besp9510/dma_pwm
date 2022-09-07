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
#include <stdlib.h>   // C Standard library
#include <stdio.h>    // C Standard I/O libary
#include <stdint.h>   // C Standard integer types
#include <stddef.h>   // C Standard type and macro definitions

// Include C POSIX libraries:
#include <unistd.h>   // Symbolic constants and types library

// Include header files:
#include "mailbox.h"      // VideoCore mailbox interface from BroadCom
#include "uncached_mem.h" // Allocate aligned memory and mapping functions

// Mailbox flag definitions:
#define MEM_FLAG_DISCARDABLE     (1 << 0)  // Can be resized to 0 at any time. Use for cached data
#define MEM_FLAG_NORMAL          (0 << 2)  // Normal allocating alias. Don't use from ARM
#define MEM_FLAG_DIRECT          (1 << 2)  // 0xC alias uncached
#define MEM_FLAG_COHERENT        (2 << 2)  // 0x8 alias. Non-allocating in L2 but coherent
#define MEM_FLAG_ZERO            (1 << 4)  // Initialize buffer to all zeros
#define MEM_FLAG_NO_INIT         (1 << 5)  // No initialization (default is initialise to all ones)
#define MEM_FLAG_HINT_PERMALOCK  (1 << 6)  // Likely to be locked for long periods of time
#define MEM_FLAG_L1_NONALLOCATING \
    (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT)  // Allocate uncached

// RAM bus to physical address:
#define BCM_RAM_BUS_TO_PHYS(addr) (addr & ~0xC0000000)

// Allocate uncached memory:
struct uncached_mem *uncached_malloc__(struct uncached_mem *block) {
    // Definitions:
    int fd; // File descriptor

    // Open mailbox
    fd = mbox_open();

    // Allocate memory
    block->mb_handle = \
        mem_alloc(fd, block->size, block->alignment, \
        MEM_FLAG_L1_NONALLOCATING);

    // Lock memory
    block->bus_addr = mem_lock(fd, block->mb_handle);

    // Map memory
    block->virt_addr = \
        mapmem(BCM_RAM_BUS_TO_PHYS(block->bus_addr), block->size);

    // Close mailbox
    mbox_close(fd);

    // Return map
    return block;
}

// Free allocated uncached memory
int uncached_free__(struct uncached_mem *block) {
    // Definitions:
    int fd; // File descriptor

     // Open mailbox
    fd = mbox_open();

    // Unmap memory
    unmapmem(block->virt_addr, block->size);

    // Unlock memory
    mem_unlock(fd, block->mb_handle);

    // Free memory
    mem_free(fd, block->mb_handle);

    // Close mailbox
    mbox_close(fd);

    // Exit with success:
    return 0;
}

// Translate virtual to physical address of uncached memory
uint32_t uncached_virt_to_bus_addr__(struct uncached_mem *block, void *ptr) {
    // Definitions:
    uint32_t offset;

    // Find offset of address to its base:
    offset = (char*)ptr - (char*)block->virt_addr;

    // Check that offset falls within allocated memory size:
    if (offset <= block->size) {
        // Return base buss address plus offset:
        return (block->bus_addr + offset);
    } else {
        // Return with error
        return -1;
    }
}