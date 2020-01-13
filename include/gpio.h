// Set GPIO function:
#define GPIO_INP(addr, p) *(addr + ((p)/10)) &= ~(7 << (((p) % 10)*3)) // Input
#define GPIO_OUT(addr, p) *(addr + ((p)/10)) |=  (1 << (((p) % 10)*3)) // Output

// Set and clear GPIO:
#define GPIO_SET(addr, p)   *(addr + 7)  = (1 << p)
#define GPIO_CLEAR(addr, p) *(addr + 10) = (1 << p)

// GPIO level:
#define GPIO_READ(addr, p) (*(addr + 13) & (1 << p))

// Make GCC happy
extern int make_iso_compilers_happy;