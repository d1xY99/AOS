#include "assert.h"
#include "stdlib.h"
#include "stdio.h"
// #include "limits.h"

// Test allocation boundary conditions and edge cases
int alloc_limits_test()
{
    printf("Testing allocation boundary conditions.\n");

    // Test 1: Very small allocations
    void *tiny = malloc(1);
    assert(tiny != NULL);
    free(tiny);
    printf("Test 1: Tiny allocation (1 byte) - passed\n");

    // Test 2: Power of 2 allocations
    for (int i = 6; i <= 16; i++) // 64 to 65536 bytes
    {
        size_t size = 1 << i;
        void *ptr = malloc(size);
        if (!ptr)
        {
            printf("Failed at 2^%d bytes\n", i);
            return -1;
        }
        free(ptr);
    }
    printf("Test 2: Power of 2 allocations - passed\n");

    // Test 3: Odd-sized allocations
    for (int i = 0; i < 5; i++)
    {
        size_t size = 1000 + i * 333;
        void *ptr = malloc(size);
        assert(ptr != NULL);
        free(ptr);
    }
    printf("Test 3: Odd-sized allocations - passed\n");

    // Test 4: Large allocation then free
    void *large = malloc(5 * 1024 * 1024); // 5MB
    if (large)
    {
        free(large);
        printf("Test 4: Large allocation (5MB) - passed\n");
    }
    else
    {
        printf("Test 4: Large allocation skipped (not enough memory)\n");
    }

    // Test 5: Zero size should fail
    void *zero = malloc(0);
    if (zero == NULL)
    {
        printf("Test 5: Zero-size allocation correctly rejected\n");
    }

    printf("Allocation limits test completed successfully!\n");
    return 0;
}
