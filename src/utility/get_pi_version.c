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
#include <string.h> // C Standard string manipulation libary

// Pi version description struct:
static struct pi_version_struct {
    char *revision_string; // "Revision" string
    int version;           // Version number
};

// Pi version struct:
static struct pi_version_struct pi_version[] = {
    // Model B Rev 1
    {
        .revision_string = "0002",
        .version = 1
    },

    // Model B Rev 1
    {
        .revision_string = "0003",
        .version = 1
    },

    // Model B Rev 2
    {
        .revision_string = "0004",
        .version = 1
    },

    // Model B Rev 2
    {
        .revision_string = "0005",
        .version = 1
    },

    // Model B Rev 2
    {
        .revision_string = "0006",
        .version = 1
    },

    // Model A
    {
        .revision_string = "0007",
        .version = 1
    },

    // Model A
    {
        .revision_string = "0008",
        .version = 1
    },

    // Model A
    {
        .revision_string = "0009",
        .version = 1
    },

    // Model B Rev 2
    {
        .revision_string = "000d",
        .version = 1
    },

    // Model B Rev 2
    {
        .revision_string = "000e",
        .version = 1
    },

    // Model B Rev 2
    {
        .revision_string = "000f",
        .version = 1
    },

    // Model B+
    {
        .revision_string = "0010",
        .version = 1
    },

    // Model B+
    {
        .revision_string = "0013",
        .version = 1
    },

    // Model B+
    {
        .revision_string = "900032",
        .version = 1
    },

    // Model A+
    {
        .revision_string = "0012",
        .version = 1
    },

    // Model A+
    {
        .revision_string = "0015",
        .version = 1
    },

    // Pi 2 Model B v1.1
    {
        .revision_string = "a01041",
        .version = 2
    },

    // Pi 2 Model B v1.1
    {
        .revision_string = "a21041",
        .version = 2
    },

    // Pi 2 Model B v1.2
    {
        .revision_string = "a22042",
        .version = 2
    },

    // Pi Zero v1.2
    {
        .revision_string = "900092",
        .version = 0
    },

    // Pi Zero v1.3
    {
        .revision_string = "900093",
        .version = 0
    },

    // Pi Zero W
    {
        .revision_string = "9000C1",
        .version = 0
    },

    // Pi 3 Model B
    {
        .revision_string = "a02082",
        .version = 3
    },

    // Pi 3 Model B
    {
        .revision_string = "a22082",
        .version = 3
    },

    // Pi 3 Model B+
    {
        .revision_string = "a020d3",
        .version = 3
    },

    // Computer Module 3
    {
        .revision_string = "a020a0",
        .version = 3
    },

    // Computer Module 3+
    {
        .revision_string = "a02100",
        .version = 3
    },

    // Pi 4
    {
        .revision_string = "a03111",
        .version = 4
    },

    // Pi 4
    {
        .revision_string = "b03111",
        .version = 4
    },

    // Pi 4
    {
        .revision_string = "c03111",
        .version = 4
    },

    // Pi 4
    {
        .revision_string = "d03114",
        .version = 4
    },

    // Pi 4
    {
        .revision_string = "b03114",
        .version = 4
    },

    // Pi 4 Model B Rev 1.5
    {
        .revision_string = "c03115",
        .version = 4
    },

    // Pi 400 Rev 1.1
    {
        .revision_string = "c03131",
        .version = 4
    }
};

// Get Raspberry Pi board version
int get_pi_version__() {
    // Definitions
    int version;          // Pi board version
    char *substring;      // Substring

    char file_line[80]; // Line of text

    int i;

    // Open file:
    FILE *fd = fopen("/proc/cpuinfo", "r");

    // Search through file:
    while (fgets(file_line, 79, fd)) {
        // Find line with "Revision" substring:
        if (strstr(file_line, "Revision")) {
            // Extract revision string:
            substring = strstr(file_line, ": ");

            // Exit loop:
            break;
        }
    }

    // Close file:
    fclose(fd);

    // Match revision substring to Pi version:
    for (i = 0; i < (sizeof(pi_version) / sizeof(struct pi_version_struct)); i++) {
        // Check if revision matches:
        if (strstr(substring, pi_version[i].revision_string) != NULL) {
            // Get version:
            version = pi_version[i].version;

            // Exit loop:
            break;
        // If found no match:
        } else if (i == (sizeof(pi_version) / sizeof(struct pi_version_struct))) {
            // Exit with error:
            return -1;
        }
    }

    // Exit with pi version
    return version;
}
