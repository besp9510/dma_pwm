// Include C standard libraries:
#include <stdio.h>  // C Standard I/O libary
#include <stdlib.h> // C Standard library
#include <string.h> // C Standard string manipulation libary

// PI version description struct:
static struct pi_version_struct {
    char *revision_string; // "Revision" string
    int version;           // Version number
};

// PI version struct:
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

    // PI 2 Model B v1.1
    {
        .revision_string = "a01041",
        .version = 2
    },

    // PI 2 Model B v1.1
    {
        .revision_string = "a21041",
        .version = 2
    },

    // PI 2 Model B v1.2
    {
        .revision_string = "a22042",
        .version = 2
    },

    // PI Zero v1.2
    {
        .revision_string = "900092",
        .version = 0
    },

    // PI Zero v1.3
    {
        .revision_string = "900093",
        .version = 0
    },

    // PI Zero W
    {
        .revision_string = "9000C1",
        .version = 0
    },

    // PI 3 Model B
    {
        .revision_string = "a02082",
        .version = 3
    },

    // PI 3 Model B
    {
        .revision_string = "a22082",
        .version = 3
    },

    // PI 3 Model B+
    {
        .revision_string = "a020d3",
        .version = 3
    },

    // PI 4
    {
        .revision_string = "a03111",
        .version = 4
    },

    // PI 4
    {
        .revision_string = "b03111",
        .version = 4
    },

    // PI 4
    {
        .revision_string = "c03111",
        .version = 4
    },
};

// Get Raspberry PI board version
int get_pi_version__() {
    // Definitions
    int version;          // PI board version
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

    // Match revision substring to PI version:
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
