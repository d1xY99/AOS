#include "assert.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

// Test allocation pattern with interleaved allocations and frees
int alloc_pattern_test()
{
    printf("Testing allocation pattern with interleaved operations.\n");

    void *ptrs[20];
    size_t sizes[20] = {0};

    // Allocate in pairs and free selectively
    for (int i = 0; i < 10; i++)
    {
        sizes[i * 2] = 256 * (i + 1);
        sizes[i * 2 + 1] = 128 * (i + 1);

        ptrs[i * 2] = malloc(sizes[i * 2]);
        ptrs[i * 2 + 1] = malloc(sizes[i * 2 + 1]);

        if (!ptrs[i * 2] || !ptrs[i * 2 + 1])
        {
            printf("Allocation failed at iteration %d\n", i);
            return -1;
        }
    }

    // Free every third allocation
    for (int i = 0; i < 20; i += 3)
    {
        if (ptrs[i])
        {
            free(ptrs[i]);
            ptrs[i] = NULL;
            sizes[i] = 0;
        }
    }

    // Verify remaining allocations are valid by writing to them
    for (int i = 0; i < 20; i++)
    {
        if (ptrs[i] && i % 3 != 0)
        {
            memset(ptrs[i], 0xCC, sizes[i]);
        }
    }

    // Clean up
    for (int i = 0; i < 20; i++)
    {
        if (ptrs[i] && i % 3 != 0)
            free(ptrs[i]);
    }

    printf("Allocation pattern test completed successfully!\n");
    return 0;
}
