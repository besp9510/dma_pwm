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