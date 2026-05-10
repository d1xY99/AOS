#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>

static int spawn_child(int exit_code)
{
  pid_t pid = fork();
  if (pid < 0)
  {
    printf("fork_multiple_wait: fork failed\n");
    return -1;
  }
  if (pid == 0)
  {
    _exit(exit_code);
  }
  return pid;
}

int fork_multiple_wait(void)
{
  printf("fork_multiple_wait: starting\n");

  int first = spawn_child(11);
  if (first < 0)
    return 1;
  int second = spawn_child(22);
  if (second < 0)
    return 1;
  assert(first > 0 && second > 0);

  int status = 0;
  if (waitpid(first, &status, 0) != first || status != 11)
  {
    printf("fork_multiple_wait: wait for first failed (status %d)\n", status);
    return 1;
  }
  assert(status == 11);

  if (waitpid(second, &status, 0) != second || status != 22)
  {
    printf("fork_multiple_wait: wait for second failed (status %d)\n", status);
    return 1;
  }
  assert(status == 22);

  printf("fork_multiple_wait: success\n");
  return 0;
}
