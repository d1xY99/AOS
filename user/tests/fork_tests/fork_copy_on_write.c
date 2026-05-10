#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>

static int shared_state = 123;

int fork_copy_on_write(void)
{
  printf("fork_copy_on_write: initial shared_state=%d\n", shared_state);

  pid_t pid = fork();
  assert(pid >= 0 && "fork_copy_on_write: fork failed");

  if (pid == 0)
  {
    shared_state = 999;
    _exit(0);
  }

  int status = -1;
  assert(waitpid(pid, &status, 0) == pid);
  assert(status == 0);

  if (shared_state != 123)
  {
    printf("fork_copy_on_write: parent saw shared_state=%d\n", shared_state);
    return 1;
  }

  printf("fork_copy_on_write: success (shared_state still %d)\n", shared_state);
  return 0;
}
