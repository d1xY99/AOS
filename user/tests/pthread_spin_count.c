#include <pthread.h>
#include <stdio.h>

size_t counter = 0;
pthread_spinlock_t lock;

#define THREAD_COUNT 300
#define ITERATIONS 100000

void *thread_function(void *arg) {
  for (size_t i = 0; i < ITERATIONS; i++) {
    pthread_spin_lock(&lock);
    counter++;
    pthread_spin_unlock(&lock);
  }
  return (void *)0;
}

int main(int argc, char *argv[]) {
  printf("Starting pthread_spin_count test with %d threads and %d iterations "
         "each\n",
         THREAD_COUNT, ITERATIONS);

  pthread_t threads[THREAD_COUNT];
  int ret;

  ret = pthread_spin_init(&lock, 0);
  if (ret != 0) {
    return -1;
  }

  for (size_t i = 0; i < THREAD_COUNT; i++) {
    ret = pthread_create(&threads[i], NULL, thread_function, NULL);
    if (ret != 0) {
      return -1;
    }
  }

  for (size_t i = 0; i < THREAD_COUNT; i++) {
    ret = pthread_join(threads[i], NULL);
    if (ret != 0) {
      return -1;
    }
  }

  ret = pthread_spin_destroy(&lock);
  if (ret != 0) {
    return -1;
  }

  if (counter != THREAD_COUNT * ITERATIONS) {
    printf("Counter value: %zu, expected: %d\n", counter,
           THREAD_COUNT * ITERATIONS);
    return -1;
  }

  printf("Test passed, counter value: %zu\n", counter);

  return 0;
}
