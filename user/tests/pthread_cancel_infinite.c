#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *infinite_loop(void *arg) {
  pthread_setcanceltype(1, NULL);

  printf("Thread started, entering infinite loop...\n");
  while (1) {
    for (volatile int i = 0; i < 1000000; ++i)
      ;
  }

  assert(0 && "Should never reach here");

  return NULL;
}

int main(void) {
  pthread_t tid;
  int ret;

  ret = pthread_create(&tid, NULL, infinite_loop, NULL);
  if (ret != 0) {
    printf("pthread_create");
    exit(-1);
  }

  sleep(2);

  printf("Canceling thread...\n");
  ret = pthread_cancel(tid);
  if (ret != 0) {
    printf("pthread_cancel");
    exit(-1);
  }

  ret = pthread_join(tid, NULL);
  if (ret != 0) {
    printf("pthread_join");
    exit(-1);
  }

  printf("Thread canceled and joined successfully.\n");
  return 0;
}
