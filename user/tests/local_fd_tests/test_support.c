#include "test_support.h"

#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "wait.h"

int reset_file_contents(const char *path, const char *payload)
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

int read_file_into_buffer(const char *path, char *buffer, int capacity)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0)
  {
    return -1;
  }

  ssize_t received = read(fd, buffer, capacity - 1);
  close(fd);
  if (received < 0)
  {
    return -1;
  }

  buffer[received] = '\0';
  return 0;
}

int wait_for_child(pid_t child_pid, int *exit_status)
{
  int status = 0;
  if (waitpid(child_pid, &status, 0) != child_pid)
  {
    return -1;
  }

  if (exit_status)
  {
    *exit_status = status;
  }
  return 0;
}
