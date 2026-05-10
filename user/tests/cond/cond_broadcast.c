#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_WAITERS 5

static pthread_mutex_t mtx;
static pthread_cond_t cond;
static int ready_flag = 0;
static int woke_count = 0;

static void *waiter(void *arg) {
  (void)arg;
  pthread_mutex_lock(&mtx);
  while (!ready_flag) {
    pthread_cond_wait(&cond, &mtx);
  }
  woke_count++;
  pthread_mutex_unlock(&mtx);
  return NULL;
}

int condBroadcast() {
  pthread_t threads[NUM_WAITERS];
  pthread_mutex_init(&mtx, NULL);
  pthread_cond_init(&cond, NULL);

  for (int i = 0; i < NUM_WAITERS; ++i) {
    if (pthread_create(&threads[i], NULL, waiter, NULL) != 0) {
      return -1;
    }
  }

  sleep(1);

  pthread_mutex_lock(&mtx);
  ready_flag = 1;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mtx);

  for (int i = 0; i < NUM_WAITERS; ++i) {
    pthread_join(threads[i], NULL);
  }

  if (woke_count != NUM_WAITERS) {
    return -2;
  }

  pthread_mutex_destroy(&mtx);
  pthread_cond_destroy(&cond);
  return 0;
}
