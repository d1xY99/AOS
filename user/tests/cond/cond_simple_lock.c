#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t lock;
pthread_cond_t cond;
int fuel = 0;

void *fueler(void *arg) {
  for (int i = 0; i < 5; i++) {
    sleep(1);

    pthread_mutex_lock(&lock);
    fuel += 20;
    printf("[Fueler] Fuel increased to %d\n", fuel);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
  }
  return NULL;
}

void *checker(void *arg) {
  pthread_mutex_lock(&lock);

  while (fuel <= 50) {
    printf("[Checker] Fuel = %d, waiting...\n", fuel);
    pthread_cond_wait(&cond, &lock);
  }

  printf("[Checker] Fuel is now %d, above 50!\n", fuel);
  fuel -= 50;
  printf("[Checker] Consumed 50 fuel, remaining fuel = %d\n", fuel);

  pthread_mutex_unlock(&lock);
  return NULL;
}

int condSimpleLock() {
  pthread_t t1, t2;

  pthread_mutex_init(&lock, NULL);
  pthread_cond_init(&cond, NULL);

  pthread_create(&t1, NULL, fueler, NULL);
  pthread_create(&t2, NULL, checker, NULL);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  pthread_mutex_destroy(&lock);
  pthread_cond_destroy(&cond);

  return 0;
}
