#include "pipe_common.h"

#define PIPE_TEST_BUF 4096
#define LARGE_TRANSFER_BYTES (PIPE_TEST_BUF * 3 + 123)

struct transfer_ctx
{
  int fd;
  size_t total;
};

static void fill_pattern(char *buffer, size_t len, size_t start)
{
  for (size_t i = 0; i < len; ++i)
    buffer[i] = (char)((start + i) & 0xFF);
}

static void *large_writer(void *arg)
{
  struct transfer_ctx *ctx = (struct transfer_ctx *)arg;
  char chunk[321];
  size_t produced = 0;
  while (produced < ctx->total)
  {
    size_t to_write = sizeof(chunk);
    if (to_write > ctx->total - produced)
      to_write = ctx->total - produced;
    fill_pattern(chunk, to_write, produced);
    ssize_t written = write(ctx->fd, chunk, to_write);
    assert(written == (ssize_t)to_write);
    produced += (size_t)written;
  }
  close(ctx->fd);
  return NULL;
}

static void *large_reader(void *arg)
{
  struct transfer_ctx *ctx = (struct transfer_ctx *)arg;
  char chunk[257];
  size_t consumed = 0;
  while (1)
  {
    ssize_t bytes = read(ctx->fd, chunk, sizeof(chunk));
    if (bytes == 0)
      break;
    assert(bytes > 0);
    for (ssize_t i = 0; i < bytes; ++i)
    {
      char expected = (char)(((consumed + (size_t)i) & 0xFF));
      assert(chunk[i] == expected);
    }
    consumed += (size_t)bytes;
  }
  assert(consumed == ctx->total);
  close(ctx->fd);
  return NULL;
}

int test_pipe_large_transfer(void)
{
  int pipefd[2];
  assert(pipe(pipefd) == 0);

  pthread_t writer;
  pthread_t reader;
  struct transfer_ctx writer_ctx = {.fd = pipefd[1], .total = LARGE_TRANSFER_BYTES};
  struct transfer_ctx reader_ctx = {.fd = pipefd[0], .total = LARGE_TRANSFER_BYTES};

  assert(pthread_create(&reader, NULL, large_reader, &reader_ctx) == 0);
  assert(pthread_create(&writer, NULL, large_writer, &writer_ctx) == 0);

  assert(pthread_join(writer, NULL) == 0);
  assert(pthread_join(reader, NULL) == 0);

  return 0;
}

int test_pipe_eof_and_epipe(void)
{
  int pipefd[2];
  assert(pipe(pipefd) == 0);

  const char message[] = "EOF test payload";
  assert(write(pipefd[1], message, sizeof(message)) == (ssize_t)sizeof(message));
  assert(close(pipefd[1]) == 0);

  char buffer[sizeof(message)];
  assert(read(pipefd[0], buffer, sizeof(buffer)) == (ssize_t)sizeof(message));
  assert(memcmp(buffer, message, sizeof(message)) == 0);
  assert(read(pipefd[0], buffer, sizeof(buffer)) == 0);
  assert(close(pipefd[0]) == 0);

  assert(pipe(pipefd) == 0);
  assert(close(pipefd[0]) == 0);
  char dummy = 'X';
  assert(write(pipefd[1], &dummy, 1) == -1);
  assert(close(pipefd[1]) == 0);

  return 0;
}

struct wait_test_ctx
{
  int pipe_fds[2];
  volatile size_t bytes_seen;
};

struct writer_block_ctx
{
  int pipe_fds[2];
  size_t total_to_write;
  volatile int writer_done;
};

static void *writer_fill_pipe(void *arg)
{
  struct writer_block_ctx *ctx = (struct writer_block_ctx *)arg;
  char chunk[PIPE_TEST_BUF];
  memset(chunk, 0xAA, sizeof(chunk));

  size_t produced = 0;
  while (produced < ctx->total_to_write)
  {
    size_t remaining = ctx->total_to_write - produced;
    size_t to_write = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    ssize_t written = write(ctx->pipe_fds[1], chunk, to_write);
    assert(written > 0);
    produced += (size_t)written;
  }

  assert(close(ctx->pipe_fds[1]) == 0);
  ctx->writer_done = 1;
  return NULL;
}

// static void busy_wait(void)
// {
//   for (volatile int i = 0; i < 100000; ++i)
//     sched_yield();
// }

int test_pipe_writer_blocks_until_reader_consumes(void)
{
  struct writer_block_ctx ctx = {.writer_done = 0,
                                 .total_to_write = PIPE_TEST_BUF * 4};
  assert(pipe(ctx.pipe_fds) == 0);

  pthread_t writer;
  assert(pthread_create(&writer, NULL, writer_fill_pipe, &ctx) == 0);

  // busy_wait();
  assert(ctx.writer_done == 0 &&
         "writer unexpectedly finished before reader drained the pipe");

  char sink[256];
  size_t consumed = 0;
  ssize_t bytes;
  while ((bytes = read(ctx.pipe_fds[0], sink, sizeof(sink))) > 0)
  {
    consumed += (size_t)bytes;
  }

  assert(pthread_join(writer, NULL) == 0);
  assert(ctx.writer_done == 1);
  assert(consumed == ctx.total_to_write);
  assert(close(ctx.pipe_fds[0]) == 0);
  return 0;
}

struct reader_block_ctx
{
  int pipe_fds[2];
  volatile int reader_waiting;
  volatile int reader_done;
  size_t bytes_read;
};

static void *reader_waiting_for_data(void *arg)
{
  struct reader_block_ctx *ctx = (struct reader_block_ctx *)arg;
  char buffer[64];
  ctx->reader_waiting = 1;
  ssize_t bytes = read(ctx->pipe_fds[0], buffer, sizeof(buffer));
  assert(bytes > 0);
  ctx->bytes_read = (size_t)bytes;
  ctx->reader_done = 1;
  return NULL;
}

int test_pipe_reader_blocks_until_writer_produces(void)
{
  struct reader_block_ctx ctx = {
      .reader_waiting = 0, .reader_done = 0, .bytes_read = 0};
  assert(pipe(ctx.pipe_fds) == 0);

  pthread_t reader;
  assert(pthread_create(&reader, NULL, reader_waiting_for_data, &ctx) == 0);

  while (!ctx.reader_waiting)
    sched_yield();

  // busy_wait();
  assert(ctx.reader_done == 0 && "reader should still be blocked");

  const char payload[] = "reader unblock data";
  assert(write(ctx.pipe_fds[1], payload, sizeof(payload)) ==
         (ssize_t)sizeof(payload));
  assert(close(ctx.pipe_fds[1]) == 0);

  assert(pthread_join(reader, NULL) == 0);
  assert(ctx.reader_done == 1);
  assert(ctx.bytes_read == sizeof(payload));
  assert(close(ctx.pipe_fds[0]) == 0);
  return 0;
}
