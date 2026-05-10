#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// Global  thread identifiers
pthread_t t1, t2;

void *thread_func1(void *arg)
{
    printf("Thread 1 started\n");

    // Try joining thread 2
    printf("Thread 1 waiting for Thread 2...\n");
    int ret = pthread_join(t2, NULL);
    if (ret != 0)
    {
        printf("pthread_join (Thread 1 -> Thread 2) failed");
    }

    printf("Thread 1 finished\n");
    return NULL;
}

void *thread_func2(void *arg)
{
    printf("Thread 2 started\n");

    // Try joining thread 1
    printf("Thread 2 waiting for Thread 1...\n");
    int ret = pthread_join(t1, NULL);
    if (ret != 0)
    {
        printf("pthread_join (Thread 2 -> Thread 1) failed");
    }

    printf("Thread 2 finished\n");
    return NULL;
}

int main(void)
{
    // Create both threads
    if (pthread_create(&t1, NULL, thread_func1, NULL) != 0)
    {
        printf("pthread_create t1");
        exit(1);
    }

    if (pthread_create(&t2, NULL, thread_func2, NULL) != 0)
    {
        printf("pthread_create t2");
        exit(1);
    }

    // Join both threads
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Main finished.\n");
    return 0;
}
