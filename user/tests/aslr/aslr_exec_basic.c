#include <stdio.h>
#include <unistd.h>
#include <wait.h>

int aslrExecBasic(void) {
  pid_t pid = fork();
  if (pid < 0) {
    printf("fork\n");
    return 1;
  }

  if (pid == 0) {
    char *argv[] = {"/usr/exec_helper.elf", 0};
    execv("/usr/exec_helper.elf", argv);
    printf("exec\n");
    _exit(1);
  }

  int status = 0;
  waitpid(pid, &status, 0);
  if (status != 0) {
    printf("child\n");
    return 1;
  }

  return 0;
}
