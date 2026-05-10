#include <pthread.h>

int condInit() {
  pthread_cond_t cond;
  int rc = pthread_cond_init(&cond, 0);
  if (rc != 0) {
    return -1;
  }

  if (cond.status != PTHREAD_COND_INIT) {
    return -1;
  }

  return 0;
}
