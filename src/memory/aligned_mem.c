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

#define PAGE_SHIFT 12
#define PAGEMAP_LENGTH 8

// Allocate aligned memory:
void *aligned_malloc(size_t size, size_t alignment) {
    // Allocate input size plus size of pointer (to store malloc returned 
    // pointer) plus alignment - 1 (room to shift return pointer to 
    // alignment):
    void *ptr_malloc = malloc(size + sizeof(void*) + (alignment - 1));
    
    // Find pointer aligned within allocated memory to desired alignment:
    // (mask address plus alignment - 1 plus size of pointer with desired
    // alignment. This mask rounds up to neareast alignment)
    void *ptr_aligned = (void**)((uint32_t) \
        (ptr_malloc + (alignment - 1) + sizeof(void*)) & ~(alignment - 1));
    
    // Store address returned by malloc immediately before aligned address
    // (void** cast is cause void* pointer cannot be decremented, cause
    // compiler has no idea "how many" bytes to decrement by)
    ((void **) ptr_aligned)[-1] = ptr_malloc;
    
    // Return user pointer
    return ptr_aligned;
}

// Free allocated aligned memory:
int aligned_free(void *ptr) {
    // Free address immeadiately before aligned address as this address was
    // returned by malloc:
    free(((void**) ptr)[-1]);

    // Return success:
    return 0;
}

// Virtual to physical memory address using pagemap:
uint32_t virt_to_phys_addr(uint32_t virt_addr, pid_t pid) {
    // Definitions:
    FILE *fd;

    int page_size; // Page size

    int pt_offset; // Page table offset
    int pfn;       // Physical page frame

    uint32_t phys_addr;

    char filename[80]; // Pagemap file path

    // Get page size:
    page_size = getpagesize();

    // Determine page table offset:
    pt_offset = ((virt_addr / page_size) * PAGEMAP_LENGTH);

    // Construct path to pagemap
    sprintf(filename, "/proc/%d/pagemap", pid);

    // Open pagemap:
    fd = fopen(filename, "rb");

    // Find page offset:
    fseek(fd, pt_offset, SEEK_SET);

    // Read 64-bit word at page offset:
    fread(&pfn, 1, (PAGEMAP_LENGTH - 1), fd);

    // Discard most significant bit to retrieve physical page
    // frame number:
    pfn &= 0x7fffffffffffff;

    // Recover physical address:
    phys_addr = (pfn * page_size) + (virt_addr % page_size);

    // Close pagemap:
    //fclose(fd);

    // Return physical address:
    return phys_addr;
}