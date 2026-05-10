#include <pthread.h>
#include <stdio.h>

int condSignalNoWaiters() {
  pthread_mutex_t mtx;
  pthread_cond_t cond;

  if (pthread_mutex_init(&mtx, NULL) != 0) {
    return -1;
  }
  if (pthread_cond_init(&cond, NULL) != 0) {
    return -2;
  }

  // No waiter threads; signal should succeed and do nothing else
  if (pthread_cond_signal(&cond) != 0) {
    return -3;
  }

  if (pthread_cond_destroy(&cond) != 0) {
    return -4;
  }

  if (pthread_mutex_destroy(&mtx) != 0) {
    return -5;
  }

  return 0;
}
