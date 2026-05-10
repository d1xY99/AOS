#include "assert.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

// Test realloc functionality with growing and shrinking
int alloc_realloc_test()
{
    printf("Testing realloc with size changes.\n");

    void *ptr = malloc(256);
    assert(ptr != NULL);

    memset(ptr, 0xAA, 256);

    // Grow the allocation
    ptr = realloc(ptr, 512);
    assert(ptr != NULL);

    // Verify old data is preserved
    unsigned char *p = (unsigned char *)ptr;
    for (int i = 0; i < 256; i++)
    {
        assert(p[i] == 0xAA);
    }

    // Fill new space
    for (int i = 256; i < 512; i++)
    {
        p[i] = 0xBB;
    }

    // Shrink allocation
    ptr = realloc(ptr, 384);
    assert(ptr != NULL);

    // Verify data is still intact
    p = (unsigned char *)ptr;
    for (int i = 0; i < 256; i++)
    {
        assert(p[i] == 0xAA);
    }

    for (int i = 256; i < 384; i++)
    {
        assert(p[i] == 0xBB);
    }

    free(ptr);
    printf("Realloc test completed successfully!\n");
    return 0;
}
