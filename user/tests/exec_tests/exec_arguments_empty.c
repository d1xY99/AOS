#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int execArgumentsEmpty() {
  printf("Executing exec_simple_param null single argument!\n");

  static char *args[] = {NULL};

  if (execv("/usr/exec_print_arguments.elf", args) == -1) {
    printf("execv failed");
    return 1;
  }

  return 0;
}
