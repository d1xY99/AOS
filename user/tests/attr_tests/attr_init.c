#include <pthread.h>

int attrInit(void) {

  pthread_attr_t attr;
  int rc = pthread_attr_init(&attr);
  if (rc != 0) {
    return rc;
  }

  // verify attr initialized correctly
  if (attr.status != PTHREAD_ATTR_INIT) {
    return -1;
  }
  if (attr.stack_size != 0) {
    return -2;
  }

  return 0;
}
