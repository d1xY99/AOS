#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>

#define MAX_NESTED_DEPTH 5

static int spawn_next(size_t depth);

int nestedFork(void)
{
  printf("nestedFork: starting nested chain to depth %d\n", MAX_NESTED_DEPTH);
  int rc = spawn_next(0);
  printf("nestedFork: finished with rc=%d\n", rc);
  return rc;
}

static int spawn_next(size_t depth)
{
  if (depth == MAX_NESTED_DEPTH)
  {
    printf("nestedFork: leaf at depth %zu exiting\n", depth);
    return 0;
  }

  printf("nestedFork: depth %zu forking child\n", depth);
  pid_t child = fork();
  assert(child >= 0 && "fork must succeed in nestedFork test");

  if (child == 0)
  {
    int rc = spawn_next(depth + 1);
    _exit(rc);
  }

  int status = -1;
  pid_t waited = waitpid(child, &status, 0);
  assert(waited == child && "parent must reap its direct child");
  assert(status == 0 && "child chain should exit cleanly");

  printf("nestedFork: depth %zu child complete\n", depth);
  if (depth == 0)
    printf("nestedFork: success\n");

  return 0;
}
