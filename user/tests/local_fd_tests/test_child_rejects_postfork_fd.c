#include "test_support.h"

#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "wait.h"

#define POSTFORK_FILE "usr/postfork_guard.txt"
#define POSTFORK_SIGNAL "usr/postfork_signal.txt"
#define POSTFORK_BUFFER 128
#define POSTFORK_ATTEMPTS 5

static int write_decimal(char *buffer, size_t capacity, int value)
{
  if (capacity == 0)
    return 0;

  char digits[16];
  int used = 0;
  unsigned int number = (value < 0) ? (unsigned int)(-value) : (unsigned int)value;

  do
  {
    digits[used++] = (char)('0' + (number % 10));
    number /= 10;
  } while (number > 0 && used < (int)sizeof(digits));

  int index = 0;
  if (value < 0 && index < (int)capacity - 1)
  {
    buffer[index++] = '-';
  }

  while (used > 0 && index < (int)capacity - 1)
  {
    buffer[index++] = digits[--used];
  }

  buffer[index] = '\0';
  return index;
}

static void child_assert_fd_invalid(int borrowed_fd)
{
  char buffer[POSTFORK_BUFFER];
  ssize_t attempt = read(borrowed_fd, buffer, sizeof(buffer));
  if (attempt >= 0)
  {
    write(1, "[FAIL] Child unexpectedly read from parent's post-fork fd\n", 59);
    write(1, buffer, attempt);
    write(1, "\n", 1);
  }
  assert(attempt == -1);

  int close_result = close(borrowed_fd);
  if (close_result == 0)
  {
    write(1, "[FAIL] Child managed to close parent's post-fork fd\n", 53);
  }
  assert(close_result == -1);
}

static void ensure_empty_signal(const char *path)
{
  unlink(path);
  int fd = open(path, O_WRONLY | O_CREAT);
  assert(fd >= 0);
  close(fd);
}

static int wait_for_fd_from_signal(const char *signal_path, int attempts)
{
  for (int attempt = 0; attempt < attempts; ++attempt)
  {
    int fd = open(signal_path, O_RDONLY);
    assert(fd >= 0);

    char buffer[POSTFORK_BUFFER];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes > 0)
    {
      buffer[bytes] = '\0';
      return atoi(buffer);
    }

    sleep(1);
  }

  assert(0 && "timed out waiting for fd signal");
  return -1;
}

static void publish_fd_value(const char *signal_path, int value)
{
  int fd = open(signal_path, O_WRONLY);
  assert(fd >= 0);

  char buffer[POSTFORK_BUFFER];
  int len = write_decimal(buffer, sizeof(buffer), value);
  assert(len > 0);

  ssize_t written = write(fd, buffer, (size_t)len);
  assert(written == len);
  close(fd);
}

static int wait_and_load_borrowed_fd(void)
{
  return wait_for_fd_from_signal(POSTFORK_SIGNAL, POSTFORK_ATTEMPTS);
}

static void publish_parent_fd(int parent_fd)
{
  publish_fd_value(POSTFORK_SIGNAL, parent_fd);
}

int scenario_child_rejects_postfork_fd(void)
{
  const char *payload = "postfork-guard\n";
  assert(reset_file_contents(POSTFORK_FILE, payload) == 0);

  ensure_empty_signal(POSTFORK_SIGNAL);

  pid_t child = fork();
  assert(child >= 0);

  if (child == 0)
  {
    int borrowed_fd = wait_and_load_borrowed_fd();
    child_assert_fd_invalid(borrowed_fd);
    _exit(0);
  }

  int parent_fd = open(POSTFORK_FILE, O_RDONLY);
  assert(parent_fd >= 0 && "parent failed to open post-fork file");

  publish_parent_fd(parent_fd);

  char verify[POSTFORK_BUFFER];
  ssize_t parent_bytes = read(parent_fd, verify, sizeof(verify) - 1);
  assert(parent_bytes >= 0);
  verify[(parent_bytes >= 0) ? parent_bytes : 0] = '\0';
  assert(strstr(verify, payload) != NULL);
  assert(close(parent_fd) == 0);

  assert(wait_for_child(child, NULL) == 0);

  printf("[child_rejects_postfork_fd] verification succeeded\n");
  return 0;
}
