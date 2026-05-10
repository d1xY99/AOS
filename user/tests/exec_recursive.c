
#include <stdio.h>

int main() {
  printf("Executing exec_simple_param with arguments!\n");

  static char *args[] = {"Test", NULL};

  if (execv("/usr/exec_recursive_print.elf", args) == -1) {
    return 1;
    printf("execv failed");
  }

  return 0;
}
