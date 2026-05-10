#include "semaphore.h"

int semSimpleLock() {

  sem_t semaphore;
  int rc = sem_init(&semaphore, 0, 1);

  if (rc != 0) {
    return -1;
  }

  // Acquire the lock
  rc = sem_wait(&semaphore);
  if (rc != 0) {
    return -1;
  }

  if (semaphore.lock_value != 0) {
    return -1;
  }

  // Release the lock
  rc = sem_post(&semaphore);
  if (rc != 0) {
    return -1;
  }

  if (semaphore.lock_value != 1) {
    return -1;
  }

  return 0;
}
