#include <pthread.h>

int attrDestroy(void) {

  pthread_attr_t attr;
  int rc = pthread_attr_init(&attr);
  if (rc != 0) {
    return rc;
  }

  rc = pthread_attr_destroy(&attr);
  if (rc != 0) {
    return rc;
  }

  // verify attr destroyed correctly
  if (attr.status != PTHREAD_ATTR_DESTROYED) {
    return -1;
  }

  return 0;
}
