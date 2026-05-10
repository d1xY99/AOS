#include "pipe_common.h"

ssize_t pipe_blocking_read(int fd, char *buffer, size_t len) {
  for (int attempt = 0; attempt < 10000; ++attempt) {
    ssize_t read_bytes = read(fd, buffer, len);
    if (read_bytes != 0)
      return read_bytes;
    sched_yield();
  }
  return 0;
}
