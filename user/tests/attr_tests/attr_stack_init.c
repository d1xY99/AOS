#include <pthread.h>

int attrStackInit(void) {

  pthread_attr_t attr;
  int rc = pthread_attr_init(&attr);
  if (rc != 0) {
    return rc;
  }

  size_t stack_size = 8192; // 8 KB
  rc = pthread_attr_setstacksize(&attr, stack_size);
  if (rc != 0) {
    return rc;
  }

  if (attr.stack_size != stack_size) {
    return -1;
  }

  stack_size = 0;

  if (pthread_attr_getstacksize(&attr, &stack_size) != 0) {
    return -1;
  }

  if (stack_size != 8192) {
    return -1;
  }

  return 0;
}
