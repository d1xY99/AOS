#include <pthread.h>
#include <stdio.h>
int main() {

  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, NULL);

  pthread_spin_lock(&mutex.spinlock);
  printf("Mutex spinlock locked successfully\n");
  pthread_spin_unlock(&mutex.spinlock);
  printf("Mutex spinlock unlocked successfully\n");

  return 0;
}
