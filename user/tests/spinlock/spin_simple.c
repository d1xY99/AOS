
#include "pthread.h"

int spinInit() {

  pthread_spinlock_t lock;

  if (pthread_spin_init(&lock, 0) != 0) {
    return -1;
  }

  return 0;
}
