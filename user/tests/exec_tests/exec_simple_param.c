#include <stdio.h>

int execSimpleParam() {
  printf("Executing exec_simple_param with arguments!\n");

  static char *args[] = {"arg1", "arg2", "arg3", NULL};

  if (execv("/usr/exec_print_arguments.elf", args) == -1) {
    printf("execv failed");
    return 1;
  }

  return 0;
}
