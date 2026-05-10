#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Worker thread that will call exec()
void *exec_thread(void *arg) {
  printf("[Thread] About to exec ...\n");

  if (execv("/usr/helloworld.elf", NULL) == -1) {
    assert(0 && "Should not reach this point\n");
    return (void *)-1;
  }

  return NULL;
}

int execSimplePthreadCreate() {
  pthread_t tid;

  printf("[Main] Creating thread that will call exec...\n");

  if (pthread_create(&tid, NULL, exec_thread, NULL) != 0) {
    printf("pthread_create failed");
    return 1;
  }

  while (1)
    ;

  pthread_join(tid, NULL);

  printf("[Main] This will not be printed if exec() succeeds.\n");
  return 0;
}
