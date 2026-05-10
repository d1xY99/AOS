#include <stdio.h>
#include <pthread.h>

void *thread(void *arg) {
    (void)arg;
    pthread_exit((void*)420);
    return 0;
}

int main() {
    pthread_t tid;
  void *ret;

    int c = pthread_create(&tid, NULL, thread, NULL);
    printf("pthread_create returned: %d -> %s\n", c, (c == 0 ? "OK" : "FAIL"));

    int j = pthread_join(tid, &ret);
    printf("pthread_join returned: %d -> %s\n", j, (j == 0 ? "OK" : "FAIL"));

    printf("join return value: %p -> %s\n", ret, (ret == (void*)420 ? "OK" : "FAIL"));
    printf("the wrong join return value: %p -> %s\n", ret, (ret == (void*)421 ? "FAIL" : "OK"));

    return 0;
}
