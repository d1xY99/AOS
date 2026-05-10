#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "wait.h"

#define FOREIGN_FILE "usr/foreign_guard.txt"
#define FOREIGN_SIGNAL "usr/foreign_fd_signal.txt"
#define FOREIGN_RELEASE "usr/foreign_release_signal.txt"
#define FOREIGN_RELEASE_TOKEN "release"
#define FOREIGN_BUFFER 128
#define FOREIGN_ATTEMPTS 10
#define FOREIGN_PADDING 8

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

static void wait_for_release_signal(void)
{
  char buffer[FOREIGN_BUFFER];
  for (;;)
  {
    int fd = open(FOREIGN_RELEASE, O_RDONLY);
    assert(fd >= 0);

    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes > 0)
    {
      buffer[bytes] = '\0';
      if (strcmp(buffer, FOREIGN_RELEASE_TOKEN) == 0)
      {
        break;
      }
    }
    sleep(1);
  }
}

static int reset_file_contents(const char *path, const char *payload)
{
  int fd = open(path, O_WRONLY | O_CREAT);
  if (fd < 0)
  {
    return -1;
  }

  size_t total = strlen(payload);
  ssize_t written = write(fd, payload, total);
  close(fd);
  return (written == (ssize_t)total) ? 0 : -1;
}

static int wait_for_child(pid_t child_pid)
{
  int status = 0;
  if (waitpid(child_pid, &status, 0) != child_pid)
  {
    return -1;
  }
  return 0;
}

static void child_assert_fd_invalid(int borrowed_fd)
{
  char buffer[FOREIGN_BUFFER];
  ssize_t attempt = read(borrowed_fd, buffer, sizeof(buffer));
  if (attempt >= 0)
  {
    write(1, "[FAIL] intruder read foreign fd\n", 32);
    write(1, buffer, attempt);
    write(1, "\n", 1);
  }
  assert(attempt == -1);

  int close_result = close(borrowed_fd);
  if (close_result == 0)
  {
    write(1, "[FAIL] intruder closed foreign fd\n", 34);
  }
  assert(close_result == -1);
}

static void ensure_empty_signal(const char *path)
{
  assert(reset_file_contents(path, "") == 0);
}

static int wait_for_fd_from_signal(const char *signal_path, int attempts)
{
  for (int attempt = 0; attempt < attempts; ++attempt)
  {
    int fd = open(signal_path, O_RDONLY);
    assert(fd >= 0);

    char buffer[FOREIGN_BUFFER];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes > 0)
    {
      buffer[bytes] = '\0';
      return atoi(buffer);
    }

    sleep(1);
  }

  assert(0 && "timed out waiting for foreign fd signal");
  return -1;
}

static void publish_fd_value(const char *signal_path, int value)
{
  int fd = open(signal_path, O_WRONLY);
  assert(fd >= 0);

  char buffer[FOREIGN_BUFFER];
  int len = write_decimal(buffer, sizeof(buffer), value);
  assert(len > 0);

  ssize_t written = write(fd, buffer, (size_t)len);
  assert(written == len);
  close(fd);
}

static void run_foreign_owner(void)
{
  const char *payload = "foreign-guardian\n";
  assert(reset_file_contents(FOREIGN_FILE, payload) == 0);

  int fillers[FOREIGN_PADDING];
  for (int i = 0; i < FOREIGN_PADDING; ++i)
  {
    fillers[i] = open(FOREIGN_FILE, O_RDONLY);
    assert(fillers[i] >= 0);
  }

  int owner_fd = open(FOREIGN_FILE, O_RDONLY);
  assert(owner_fd >= 0);

  for (int i = 0; i < FOREIGN_PADDING; ++i)
  {
    assert(close(fillers[i]) == 0);
  }

  publish_fd_value(FOREIGN_SIGNAL, owner_fd);

  wait_for_release_signal();

  close(owner_fd);
  _exit(0);
}

int main(void)
{
  ensure_empty_signal(FOREIGN_SIGNAL);
  ensure_empty_signal(FOREIGN_RELEASE);

  pid_t owner = fork();
  assert(owner >= 0);

  if (owner == 0)
  {
    run_foreign_owner();
  }

  int foreign_fd = wait_for_fd_from_signal(FOREIGN_SIGNAL, FOREIGN_ATTEMPTS);

  pid_t intruder = fork();
  assert(intruder >= 0);

  if (intruder == 0)
  {
    child_assert_fd_invalid(foreign_fd);
    _exit(0);
  }

  assert(wait_for_child(intruder) == 0);

  assert(reset_file_contents(FOREIGN_RELEASE, FOREIGN_RELEASE_TOKEN) == 0);

  assert(wait_for_child(owner) == 0);

  printf("[foreign_fd_isolation] verification completed\n");
  return 0;
}
