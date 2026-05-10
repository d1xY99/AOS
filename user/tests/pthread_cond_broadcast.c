#include <stdio.h>
#include <pthread.h>

pthread_mutex_t mtx;
pthread_cond_t cv;

int ready = 0;
int woken = 0;

void* thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&mtx);

    while(!ready)
        pthread_cond_wait(&cv, &mtx);

    woken++;
    pthread_mutex_unlock(&mtx);
    return NULL;
}

int main(void) {
    pthread_t t[4];

    for (int i = 0; i < 4; i++) {
        int ret = pthread_create(&t[i], NULL, thread, NULL);
        printf("pthread_create[%d]: %d -> %s\n", i, ret, (ret == 0 ? "OK" : "FAIL"));
    }

    pthread_mutex_lock(&mtx);
    ready = 1;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);

    for (int i = 0; i < 4; i++)
        pthread_join(t[i], NULL);

    printf("Threads woken: %d -> %s\n", woken, (woken == 4 ? "OK" : "FAIL"));

    return 0;
}
