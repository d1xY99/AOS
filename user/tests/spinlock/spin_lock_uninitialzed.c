#include "pthread.h"
#include <stdio.h>

int spinLockUnitialized() {
  pthread_spinlock_t lock;

  if (pthread_spin_lock(&lock) != 0) {
    return 0;
  }

  return -1;
}
