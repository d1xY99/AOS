
#include <pthread.h>
#include <stdio.h>

#define MAX_THREADS 4

__thread int thread_local_var = 0;

void *thread_func(void *arg) {
  int thread_id = *((int *)arg);
  thread_local_var = thread_id * 10;
  printf("Thread %d: thread_local_var = %d\n", thread_id, thread_local_var);

  while (1) {
    printf("Thread %d: thread_local_var = %d\n", thread_id, thread_local_var);
    printf("Thread %d: Address of thread_local_var = %p\n", thread_id,
           (void *)&thread_local_var);
    sleep(1);
  }

  return NULL;
}

int main() {
  pthread_t threads[MAX_THREADS];

  int thread_ids[MAX_THREADS];
  for (int i = 0; i < MAX_THREADS; i++) {
    thread_ids[i] = i;
    pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]);
  }

  for (int i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}
