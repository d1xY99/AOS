#include "pthread.h"
#include "stdio.h"
#include "unistd.h"

int main(int argc, char *argv[]) {

  printf("Starting pthread_create_simple_1 test...\n");

  pthread_t thread;
  int ret = pthread_create(&thread, NULL, (void *(*)(void *))NULL, NULL);
  if (ret != 0) {
    printf(
        "Testcase success - pthread_create failed as expected with NULL start "
        "routine\n");
    return 1;
  }

  printf("Testcase failed - pthread_create should not succeed with NULL start "
         "routine\n");
  return 0;
}
