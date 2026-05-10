#include <semaphore.h>

int semInit() {
  sem_t semaphore;
  int rc = sem_init(&semaphore, 0, 1);
  if (rc != 0) {
    return -1;
  }

  if (semaphore.status != SEM_INIT) {
    return -2;
  }

  if (semaphore.lock_value != 1) {
    return -3;
  }

  return 0;
}
