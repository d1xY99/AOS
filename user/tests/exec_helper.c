#include <stdio.h>
#include <pthread.h>

void *thread_fn(void *arg)
{
    int local;
    printf("[child] Thread stack address: %p\n", &local);
    return NULL;
}

int main()
{
    int main_local = 0;
    printf("[child] Main stack address: %p\n", &main_local);

    pthread_t t;
    pthread_create(&t, NULL, thread_fn, NULL);
    pthread_join(t, NULL);

    return 0;
}
