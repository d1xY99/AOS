#include <semaphore.h>
#include <stdio.h>

int semDestroyUnitialized() {
  sem_t semaphore;
  int result = sem_destroy(&semaphore);

  if (result == -1) {
    printf("sem_destroy failed as expected on uninitialized semaphore.\n");
    return 0;
  } else {
    printf("sem_destroy succeeded unexpectedly on uninitialized semaphore.\n");
    return -1;
  }

  return 0;
}
