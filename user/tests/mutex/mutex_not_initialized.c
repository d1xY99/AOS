#include <pthread.h>

int mutexNotInitialized() {
  pthread_mutex_t lock;

  int retvals[2];

  retvals[0] = pthread_mutex_lock(&lock);
  retvals[1] = pthread_mutex_unlock(&lock);

  for (int i = 0; i < 2; i++) {
    if (retvals[i] != -1) {
      return -1;
    }
  }

  return 0;
}
