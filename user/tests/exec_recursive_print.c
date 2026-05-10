
#include <stdio.h>

int main(int argc, char *argv[]) {
  printf("Exec Recursive Print: Value = %s\n", argv[0]);

  if (execv("/usr/exec_recursive_print.elf", argv) == -1) {
    printf("execv failed\n");
    return 1;
  }

  return 0;
}
