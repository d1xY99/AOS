#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void *thread_function(void *arg)
{
    int thread_id = *((int *)arg);
    printf("Thread %d started\n", thread_id);

    // while (1)
    // {
    //     printf("Thread %d running...\n", thread_id);
    //     sleep(1); // Sleep to make output readable
    // }

    // This should never be reached if exit() works correctly
    printf("Thread %d exiting normally\n", thread_id);
    return NULL;
}

int main()
{
    printf("Multithreaded exit() test starting\n");

    int NUM_THREADS = 3;
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
    {
        thread_ids[i] = i + 1;
        if (pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]) != 0)
        {
            printf("Failed to create thread %d\n", i + 1);
            // exit(1);
        }
    }

    sleep(3); // Let threads run for a bit

    printf("Main thread calling exit(99)\n");
    exit(99);

    // This should never be printed
    printf("Main thread continued after exit\n");
    return 0;
}