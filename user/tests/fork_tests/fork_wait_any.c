#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>

int fork_wait_any(void)
{
  const int exit_codes[] = {5, 7, 9};
  const size_t children = sizeof(exit_codes) / sizeof(exit_codes[0]);
  pid_t pids[children];

  printf("fork_wait_any: spawning %zu children\n", children);

  for (size_t i = 0; i < children; ++i)
  {
    pid_t pid = fork();
    assert(pid >= 0 && "fork_wait_any: fork failed");
    if (pid == 0)
    {
      _exit(exit_codes[i]);
    }
    pids[i] = pid;
  }

  // wait in a different order than forked to ensure order independence
  const size_t wait_order[] = {1, 0, 2};
  for (size_t k = 0; k < children; ++k)
  {
    size_t idx = wait_order[k];
    int status = -1;
    pid_t waited = waitpid(pids[idx], &status, 0);
    assert(waited == pids[idx]);
    assert(status == exit_codes[idx]);
    printf("fork_wait_any: reaped pid %d status %d\n", (int)waited, status);
  }

  printf("fork_wait_any: success\n");
  return 0;
}
