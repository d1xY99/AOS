#include <stdio.h>
#include <unistd.h>
// #include <sys/wait.h>

int main() {
  printf("Testing fork() system call\n");
  size_t pid = fork();

  if (pid < 0) {
    printf("Fork failed\n");
    return 1;
  } else if (pid == 0) {
    // Child process
    printf("hello from child process! My PID is \n");
  } else {
    // Parent process
    printf("hello from parent process! My child's PID is \n");
  }
  printf("PID: %zu\n", pid);
  return 0;
}
