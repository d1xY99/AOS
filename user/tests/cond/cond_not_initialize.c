#include <pthread.h>
#include <stdio.h>

int condNotInitalized() {
  pthread_cond_t cond;

  int retvals[3];

  retvals[0] = pthread_cond_wait(&cond, NULL);
  retvals[1] = pthread_cond_signal(&cond);
  retvals[2] = pthread_cond_broadcast(&cond);

  for (int i = 0; i < 3; i++) {
    if (retvals[i] != -1) {
      return -1;
    }
  }

  return 0;
}
