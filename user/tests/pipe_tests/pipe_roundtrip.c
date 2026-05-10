#include "pipe_common.h"

int test_roundtrip_single_thread(void) {
  int fds[2];
  assert(pipe(fds) == 0 && "pipe creation failed");

  const char message[] = "roundtrip-basic";
  ssize_t written = write(fds[1], message, sizeof(message));
  assert(written == (ssize_t)sizeof(message) &&
         "write returned wrong size");

  char buffer[32] = {0};
  ssize_t read_bytes = pipe_blocking_read(fds[0], buffer, sizeof(message));
  assert(read_bytes == (ssize_t)sizeof(message) &&
         "read returned wrong size");
  assert(strcmp(buffer, message) == 0 && "payload mismatch");

  assert(close(fds[0]) == 0 && "close read end");
  assert(close(fds[1]) == 0 && "close write end");
  return 0;
}

struct writer_args {
  int fd;
  const char *msg;
  size_t len;
  ssize_t result;
};

struct reader_args {
  int fd;
  char *buf;
  size_t len;
  ssize_t result;
};

static void *writer_thread(void *arg) {
  struct writer_args *w = (struct writer_args *)arg;
  w->result = write(w->fd, w->msg, w->len);
  return NULL;
}

static void *reader_thread(void *arg) {
  struct reader_args *r = (struct reader_args *)arg;
  r->result = pipe_blocking_read(r->fd, r->buf, r->len);
  return NULL;
}

int test_threaded_roundtrip(void) {
  int fds[2];
  assert(pipe(fds) == 0 && "pipe creation failed");

  const char message[] = "threaded-delivery";
  char buffer[64] = {0};

  struct writer_args wargs = {
      .fd = fds[1], .msg = message, .len = sizeof(message), .result = -1};
  struct reader_args rargs = {
      .fd = fds[0], .buf = buffer, .len = sizeof(message), .result = -1};

  pthread_t writer;
  pthread_t reader;
  assert(pthread_create(&reader, NULL, reader_thread, &rargs) == 0 &&
         "reader thread creation failed");
  assert(pthread_create(&writer, NULL, writer_thread, &wargs) == 0 &&
         "writer thread creation failed");

  assert(pthread_join(writer, NULL) == 0 && "writer join failed");
  assert(pthread_join(reader, NULL) == 0 && "reader join failed");

  assert(wargs.result == (ssize_t)sizeof(message) && "writer wrong size");
  assert(rargs.result == (ssize_t)sizeof(message) && "reader wrong size");
  assert(strcmp(buffer, message) == 0 && "threaded payload mismatch");

  assert(close(fds[0]) == 0 && "close read end");
  assert(close(fds[1]) == 0 && "close write end");
  return 0;
}
