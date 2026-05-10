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

int aslrTest2()
{
    int main_local = 0;
    printf("Main stack addr: %p\n", &main_local);

    pthread_t t;
    pthread_create(&t, NULL, thread_func, NULL);
    pthread_join(t, NULL);
    return 0;
}
