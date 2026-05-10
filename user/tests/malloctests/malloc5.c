#include "assert.h"
#include "stdlib.h"
#include "stdio.h"
#include "pthread.h"
#include "string.h"

#define NUM_THREADS 8
#define ITERATIONS 50

// pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
int allocation_count = 0;

void *concurrent_allocator(void *arg)
{
    int thread_id = (intptr_t)arg;
    void *local_ptrs[ITERATIONS];

    for (int i = 0; i < ITERATIONS; i++)
    {
        size_t alloc_size = (thread_id * 17 + i * 13) % 2048 + 32;
        local_ptrs[i] = malloc(alloc_size);

        if (!local_ptrs[i])
        {
            printf("Thread %d: allocation failed at iteration %d\n", thread_id, i);
            return (void *)-1;
        }

        // Mark memory with thread ID
        memset(local_ptrs[i], thread_id & 0xFF, alloc_size);

        // pthread_mutex_lock(&alloc_mutex);
        allocation_count++;
        // pthread_mutex_unlock(&alloc_mutex);
    }

    // Free all allocations
    for (int i = 0; i < ITERATIONS; i++)
    {
        free(local_ptrs[i]);
    }

    return NULL;
}

// Test concurrent allocations from multiple threads
int alloc_concurrent_test()
{
    printf("Testing concurrent thread allocations.\n");

    pthread_t threads[NUM_THREADS];
    allocation_count = 0;

    // Create multiple threads doing allocations
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, concurrent_allocator, (void *)(intptr_t)i);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    printf("Total allocations performed: %d\n", allocation_count);
    assert(allocation_count == NUM_THREADS * ITERATIONS);

    printf("Concurrent allocation test completed successfully!\n");
    return 0;
}
