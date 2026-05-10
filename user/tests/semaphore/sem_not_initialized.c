#include <semaphore.h>

int semNotInitialized() {

  sem_t sem;

  int retvals[3];

  retvals[0] = sem_wait(&sem);
  retvals[1] = sem_post(&sem);
  retvals[2] = sem_trywait(&sem);

  for (int i = 0; i < 3; i++) {
    if (retvals[i] != -1) {
      return -1;
    }
  }

  return 0;
}
