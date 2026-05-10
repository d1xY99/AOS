#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
// #include <stdint.h>
#include <stdlib.h>

// Function to print the address of a local variable (i.e., stack pointer)
static void *thread_func(void *arg)
{
    int local_var = 0;
    printf("Thread %ld stack address: %p\n", (long)arg, &local_var);
    return NULL;
}

int aslrTest()
{
    printf("=== ASLR STACK TEST ===\n");

    for (int i = 0; i < 5; i++)
    {
        pthread_t th;
        pthread_create(&th, NULL, thread_func, (void *)(long)i);
        pthread_join(th, NULL);
    }

    printf("Run this test multiple times — if ASLR works, the addresses should differ each run.\n");

    return 0;
}
