#include "sched.h"
#include <pthread.h>

#include <stdio.h>

void *exit_function(void *arg) {
  while (1)
    sched_yield();
  return NULL;
}

int main(void) {

  pthread_t tid;

  while (1) {
    int rv = pthread_create(&tid, NULL, exit_function, NULL);
    if (rv != 0) {
      printf("pthread_create failed: %d\n", rv);
      // optional: break if you want to stop on error
      continue;
    }
  }

  return 0;
}
