#include <pthread.h>

int condDestroy() {
  pthread_cond_t cond;

  if (pthread_cond_init(&cond, 0) != 0) {
    return -1;
  }

  if (pthread_cond_destroy(&cond) != 0) {
    return -1;
  }

  if (cond.status != PTHREAD_COND_DESTROYED) {
    return -1;
  }

  return 0;
}
