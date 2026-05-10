

#include <stdio.h>
int main() {

  while (1) {

    pid_t pid = fork();
    if (pid == 0) {
      // Child process
      if (execv("/usr/yield_exit.elf", NULL) == -1) {
        printf("Error executing /usr/yield_exit\n");
        return -1;
      }
    } else {
      // Parent process
      int status;
      waitpid(pid, &status, 0);
    }
  }

  return 0;
}
