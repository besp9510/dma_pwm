// Allocate aligned memory
void *aligned_malloc(size_t size, size_t alignment);

// Free allocated aligned memory
int aligned_free(void *ptr);

// Virtual to physical memory address:
uintptr_t virt_to_phys_addr(uintptr_t virt_addr, pid_t pid);