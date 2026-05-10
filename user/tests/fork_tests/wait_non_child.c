#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>

int wait_non_child(void)
{
  int status = 0;
  pid_t invalid = 9999; // arbitrary pid that is very unlikely to exist
  pid_t result = waitpid(invalid, &status, 0);
  if (result != -1)
  {
    printf("wait_non_child: expected -1, got %d\n", (int)result);
    return 1;
  }
  assert(result == -1);

  printf("wait_non_child: success, invalid wait rejected\n");
  return 0;
}
