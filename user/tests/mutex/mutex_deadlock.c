// deadlock.c
#include <assert.h>
#define _XOPEN_SOURCE 700
#include <pthread.h>
#include <stdio.h>
#include <unistd.h> // usleep

static pthread_mutex_t m1;
static pthread_mutex_t m2;

static void *worker_a(void *_) {
  printf("A: trying locking m1\n");
  pthread_mutex_lock(&m1);
  printf("A: locked m1\n");
  sleep(2); // give B time to lock m2
  printf("A: trying locking m2\n");
  pthread_mutex_lock(&m2); // deadlock here
  printf("A: Deadlock got detected!\n");

  pthread_mutex_unlock(&m1);
  pthread_mutex_unlock(&m2);
  return NULL;
}

static void *worker_b(void *_) {
  printf("B: trying locking m2\n");
  pthread_mutex_lock(&m2);
  printf("B: locked m2\n");
  sleep(2);
  printf("B: trying locking m1\n");
  pthread_mutex_lock(&m1); // deadlock here
  printf("B: Deadlock got detected!\n");

  pthread_mutex_unlock(&m2);
  pthread_mutex_unlock(&m1);
  return NULL;
}

int mutexDeadlock(void) {
  pthread_t ta, tb;

  pthread_mutex_init(&m1, NULL);
  pthread_mutex_init(&m2, NULL);

  pthread_create(&ta, NULL, worker_a, NULL);
  pthread_create(&tb, NULL, worker_b, NULL);

  // These joins will never return due to the deadlock.
  pthread_join(ta, NULL);
  pthread_join(tb, NULL);

  pthread_mutex_destroy(&m1);
  pthread_mutex_destroy(&m2);
  return 0;
}
