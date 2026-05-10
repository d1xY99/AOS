#include <pthread.h>
#include <stdio.h>

pthread_mutex_t mutexLockVar;

int mutexRelock() {

  printf("Starting pthread mutex relock test\n");

  pthread_mutex_init(&mutexLockVar, NULL);

  // prinf the mutex struct
  printf("Mutex struct: locked=%d, status=%d, owner_thread=%p, spin=%d\n",
         mutexLockVar.locked, mutexLockVar.status, mutexLockVar.owner_thread,
         mutexLockVar.spinlock.lock);

  printf("Locking mutex first time...\n");
  pthread_mutex_lock(&mutexLockVar);
  printf("Locking mutex second time...\n");
  if (pthread_mutex_lock(&mutexLockVar) != -1) {
    printf("Error: Mutex relock did not return error as expected\n");
    return -1;
  }
  printf("Locking mutex third time...\n");
  if (pthread_mutex_lock(&mutexLockVar) != -1) {
    printf("Error: Mutex relock did not return error as expected\n");
    return -1;
  }

  printf("Unlocking mutex first time...\n");
  pthread_mutex_unlock(&mutexLockVar);
  printf("Unlocking mutex second time...\n");
  if (pthread_mutex_unlock(&mutexLockVar) != -1) {
    printf("Error: Mutex unlock did not return error as expected\n");
    return -1;
  }
  printf("Unlocking mutex third time...\n");
  if (pthread_mutex_unlock(&mutexLockVar) != -1) {
    printf("Error: Mutex unlock did not return error as expected\n");
    return -1;
  }

  pthread_mutex_destroy(&mutexLockVar);

  printf("Pthread mutex relock test completed successfully\n");
  return 0;
}
