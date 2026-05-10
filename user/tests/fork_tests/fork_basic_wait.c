#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>

int fork_basic_wait(void)
{
  printf("fork_basic_wait: parent starting\n");

  pid_t pid = fork();
  if (pid < 0)
  {
    printf("fork_basic_wait: fork failed\n");
    return 1;
  }

  if (pid == 0)
  {
    printf("fork_basic_wait: child running\n");
    _exit(7);
  }
  assert(pid > 0);

  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  if (waited != pid)
  {
    printf("fork_basic_wait: waitpid returned %d\n", (int)waited);
    return 1;
  }
  if (status != 7)
  {
    printf("fork_basic_wait: expected 7, got %d\n", status);
    return 1;
  }
  assert(waited == pid);
  assert(status == 7);

  printf("fork_basic_wait: success (status %d)\n", status);
  return 0;
}
