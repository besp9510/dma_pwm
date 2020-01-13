// Uncached memory structure:
struct uncached_mem {
    size_t size;         // Memory allocation size
    size_t alignment;   // Memory alignment
    uintptr_t mb_handle; // Mailbox handle
    uintptr_t bus_addr;  // Allocated memory bus address
    void *virt_addr;     // Pointer to memory virtual address
};

// Allocate uncached memory
struct uncached_mem *uncached_malloc__(struct uncached_mem *block);

// Free allocated uncached memory
int uncached_free__(struct uncached_mem *block);

// Translate virtual to physical address of uncached memory
uintptr_t uncached_virt_to_bus_addr__(struct uncached_mem *block, void *ptr);