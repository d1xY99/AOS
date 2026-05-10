#include "assert.h"
#include "stdlib.h"
#include "stdio.h"
#include "time.h"

// Test randomized allocation patterns
int alloc_randomized_test()
{
    printf("Testing randomized allocation patterns.\n");

    void *ptrs[50];
    size_t sizes[50];

    // Randomly allocate
    for (int i = 0; i < 50; i++)
    {
        sizes[i] = ((i * 17 + 13) % 256) * 16 + 64; // Pseudo-random using LCG
        ptrs[i] = malloc(sizes[i]);

        if (!ptrs[i])
        {
            printf("Failed to allocate at index %d\n", i);
            return -1;
        }
    }

    // Randomly free approximately half
    for (int i = 0; i < 50; i += 2)
    {
        free(ptrs[i]);
        ptrs[i] = NULL;
    }

    // Randomly reallocate freed slots with different sizes
    for (int i = 0; i < 50; i += 2)
    {
        sizes[i] = ((i * 7 + 11) % 256) * 8 + 32;
        ptrs[i] = malloc(sizes[i]);
        assert(ptrs[i] != NULL);
    }

    // Clean up all
    for (int i = 0; i < 50; i++)
    {
        if (ptrs[i])
            free(ptrs[i]);
    }

    printf("Randomized allocation test completed successfully!\n");
    return 0;
}
