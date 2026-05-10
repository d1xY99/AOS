#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  int fds[2];
  assert(pipe(fds) == 0);

  const int target_fd = 42;
  assert(dup2(fds[1], target_fd) == target_fd);

  // closing the old descriptor must not break the duplicated one
  assert(close(fds[1]) == 0);

  const char msg[] = "dup2-success";
  assert(write(target_fd, msg, sizeof(msg)) == (ssize_t)sizeof(msg));

  char buffer[32] = {0};
  assert(read(fds[0], buffer, sizeof(buffer)) == (ssize_t)sizeof(msg));
  assert(strcmp(buffer, msg) == 0);

  // dup2 should be a no-op when both descriptors match
  assert(dup2(target_fd, target_fd) == target_fd);

  assert(close(target_fd) == 0);
  assert(close(fds[0]) == 0);

  printf("dup2_basic passed\n");
  return 0;
}
