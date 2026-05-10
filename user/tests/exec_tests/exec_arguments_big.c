#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int execArgumentsBig() {
  printf("Executing exec_arguments_big with arguments!\n");

  char big_arg[4096 / 2];
  for (int i = 0; i < sizeof(big_arg) - 1; i++) {
    big_arg[i] = 'A';
  }
  big_arg[sizeof(big_arg) - 1] = '\0';

  // more than 4096 bytes of arguments
  char *args[] = {big_arg, big_arg, big_arg, NULL};

  if (execv("/usr/exec_print_arguments.elf", args) == -1) {
    printf("execv failed");
    // should fail due to too big arguments
    return 0;
  }

  return 1;
}
