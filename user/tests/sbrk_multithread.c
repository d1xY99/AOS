
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define THREADS 4
#define PAGES_PER_THREAD 8

static void *worker(void *arg) {
  size_t tid = *(size_t *)arg;

  for (int i = 0; i < PAGES_PER_THREAD; i++) {
    char *page = (char *)sbrk(PAGE_SIZE);
    if (page == (void *)-1) {
      printf("[T%zu] sbrk failed on iter %d\n", tid, i);
      return (void *)-1;
    }
    *page = (char)tid;
  }

  return NULL;
}

int main(void) {
  printf("Starting multithreaded sbrk test: %d threads x %d pages\n", THREADS,
         PAGES_PER_THREAD);

  pthread_t t[THREADS];
  size_t ids[THREADS];

  for (size_t i = 0; i < THREADS; i++) {
    ids[i] = i;
    int rc = pthread_create(&t[i], NULL, worker, &ids[i]);
    if (rc != 0) {
      printf("pthread_create failed for thread %zu\n", i);
      return 1;
    }
  }

  int ok = 1;
  for (int i = 0; i < THREADS; i++) {
    void *ret = NULL;
    if (pthread_join(t[i], &ret) != 0) {
      printf("pthread_join failed for thread %d\n", i);
      return 1;
    }
    if (ret == (void *)-1)
      ok = 0;
  }

  if (!ok) {
    printf("sbrk multithread test FAILED\n");
    return 1;
  }

  printf("sbrk multithread test PASSED\n");
  return 0;
}
