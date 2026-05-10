#include "pipe_common.h"

int test_pipe_duplication(void) {
  int fds[2];
  assert(pipe(fds) == 0 && "pipe creation failed");

  int dup_fd = dup(fds[1]);
  assert(dup_fd >= 0 && "dup failed");

  assert(close(fds[1]) == 0 && "close original write end");

  const char msg[] = "dup-still-works";
  ssize_t written = write(dup_fd, msg, sizeof(msg));
  assert(written == (ssize_t)sizeof(msg) && "dup write wrong size");

  char buffer[64] = {0};
  ssize_t read_bytes = pipe_blocking_read(fds[0], buffer, sizeof(msg));
  assert(read_bytes == (ssize_t)sizeof(msg) && "dup read wrong size");
  assert(strcmp(buffer, msg) == 0 && "dup payload mismatch");

  assert(close(fds[0]) == 0 && "close read end");
  assert(close(dup_fd) == 0 && "close duplicated write end");
  return 0;
}

int test_pipe_close_propagates(void) {
  int fds[2];
  assert(pipe(fds) == 0 && "pipe creation failed");

  assert(close(fds[0]) == 0 && "close read end");

  const char msg[] = "should-not-write";
  ssize_t written = write(fds[1], msg, sizeof(msg));
  assert(written < 0 && "write succeeded even though reader closed");

  assert(close(fds[1]) == 0 && "close write end");
  return 0;
}
