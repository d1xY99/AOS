#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

// Test various allocation sizes from small to large
int alloc_sizes_test()
{
    printf("Testing various allocation sizes.\n");

    size_t test_sizes[] = {1, 7, 15, 32, 64, 128, 256, 512, 1024,
                           2048, 4096, 8192, 16384, 32768, 65536, 131072};
    void *ptrs[16];

    // Allocate with different sizes
    for (int i = 0; i < 16; i++)
    {
        ptrs[i] = malloc(test_sizes[i]);
        if (!ptrs[i])
        {
            printf("Failed to allocate %lu bytes\n", test_sizes[i]);
            return -1;
        }

        // Write pattern to verify allocation
        unsigned char *p = (unsigned char *)ptrs[i];
        for (size_t j = 0; j < test_sizes[i]; j++)
        {
            p[j] = (i + j) % 256;
        }
    }

    // Verify all allocations
    for (int i = 0; i < 16; i++)
    {
        unsigned char *p = (unsigned char *)ptrs[i];
        for (size_t j = 0; j < test_sizes[i]; j++)
        {
            assert(p[j] == (i + j) % 256);
        }
    }

    // Free all
    for (int i = 0; i < 16; i++)
    {
        free(ptrs[i]);
    }

    printf("Allocation sizes test completed successfully!\n");
    return 0;
}
