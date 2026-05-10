

#include <stdio.h>

int main() {
  printf("Executing exec_fail with arguments!\n");

  static char *args[] = {"Test", NULL};

  while (1) {
    if (execv("/usr/abcd", args) == -1) {
      printf("execv failed\n");
    }
  }

  return 0;
}
