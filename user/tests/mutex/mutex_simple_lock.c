#include <pthread.h>
#include <stdio.h>

int mutexLockSimple() {
  printf("Starting mutex initialization test\n");
  pthread_mutex_t mutex;

  // Initialize the mutex
  if (pthread_mutex_init(&mutex, NULL) != 0) {
    printf("Error initializing mutex\n");
    return -1;
  }
  printf("Mutex initialized successfully\n");

  printf("Mutex struct: locked=%d, status=%d, owner_thread=%p, spin=%d\n",
         mutex.locked, mutex.status, mutex.owner_thread, mutex.spinlock.lock);

  printf("Locking the mutex...\n");
  pthread_mutex_lock(&mutex);
  printf("Mutex locked successfully\n");
  pthread_mutex_unlock(&mutex);
  printf("Mutex unlocked successfully\n");

  pthread_mutex_destroy(&mutex);
  printf("Mutex destroyed successfully\n");

  return 0;
}
