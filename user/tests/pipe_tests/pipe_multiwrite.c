#include "pipe_common.h"

int test_pipe_reuse_after_new_writes(void) {
  int fds[2];
  assert(pipe(fds) == 0 && "pipe creation failed");

  const char *messages[] = {"first-message", "second-message",
                            "third-message"};
  char buffer[64] = {0};

  for (size_t i = 0; i < PIPE_ARRAY_SIZE(messages); ++i) {
    size_t len = strlen(messages[i]) + 1;
    ssize_t written = write(fds[1], messages[i], len);
    assert(written == (ssize_t)len && "write wrong size");

    ssize_t read_bytes = pipe_blocking_read(fds[0], buffer, len);
    assert(read_bytes == (ssize_t)len && "read wrong size");
    assert(strcmp(buffer, messages[i]) == 0 && "payload mismatch");
  }

  assert(close(fds[0]) == 0 && "close read end");
  assert(close(fds[1]) == 0 && "close write end");
  return 0;
}
