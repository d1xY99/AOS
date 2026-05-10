#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>

#define INIT_COUNT 3
#define THREADS 8
#define LOOPS 25

static sem_t sem;
static int active = 0;
static int errors = 0;

static void *worker(void *arg) {
  (void)arg;
  for (int i = 0; i < LOOPS; ++i) {
    if (sem_wait(&sem) != 0) {
      errors = 1;
      break;
    }
    active++;
    if (active < 1 || active > INIT_COUNT) {
      errors = 2;
    }
    sleep(1);
    active--;
    if (sem_post(&sem) != 0) {
      errors = 3;
      break;
    }
  }
  return NULL;
}

int semCounting() {
  if (sem_init(&sem, 0, INIT_COUNT) != 0)
    return -1;
  pthread_t tids[THREADS];
  for (int i = 0; i < THREADS; ++i) {
    if (pthread_create(&tids[i], NULL, worker, NULL) != 0)
      return -2;
  }
  for (int i = 0; i < THREADS; ++i)
    pthread_join(tids[i], NULL);
  if (errors != 0)
    return -10 - errors;
  if (active != 0)
    return -20; // should be balanced
  if (sem_destroy(&sem) != 0)
    return -30;
  return 0;
}
