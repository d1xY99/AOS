#include <semaphore.h>

int semTryWait() {
  sem_t sem;
  if (sem_init(&sem, 0, 2) != 0) {
    return -1;
  }

  if (sem_trywait(&sem) != 0) { // value -> 1
    return -2;
  }

  if (sem_trywait(&sem) != 0) { // value -> 0
    return -3;
  }

  if (sem_trywait(&sem) != -1) { // should fail
    return -4;
  }

  if (sem_post(&sem) != 0) {
    return -5;
  }

  if (sem_destroy(&sem) != 0) {
    return -6;
  }
  return 0;
}
