#include "test_support.h"

#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "wait.h"

#define CLOSE_FILE "usr/close_guard_record.txt"
#define CLOSE_BUFFER 256

int scenario_double_close_guard(void)
{
  printf("[double_close_guard] reset log file\n");
  /* Start with deterministic payload that both processes can read. */
  const char *preface = "guardian-baseline\n";
  assert(reset_file_contents(CLOSE_FILE, preface) == 0);

  int fd = open(CLOSE_FILE, O_RDWR);
  assert(fd >= 0);

  pid_t child = fork();
  assert(child >= 0);

  if (child == 0)
  {
    /* Child confirms the descriptor is still valid after parent closes later. */
    char child_data[CLOSE_BUFFER];
    ssize_t first_read = read(fd, child_data, sizeof(child_data) - 1);
    assert(first_read > 0);
    child_data[first_read] = '\0';
    size_t expect_len = strlen(preface);
    for (size_t i = 0; i < expect_len; ++i)
    {
      assert(child_data[i] == preface[i]);
    }

    const char *suffix = "child-ack-close\n";
    ssize_t written = write(fd, suffix, strlen(suffix));
    assert(written == (ssize_t)strlen(suffix));

    close(fd);
    _exit(0);
  }

  /* Parent closes once legitimately and expects the second attempt to fail. */
  printf("[double_close_guard] parent closing descriptor twice\n");
  assert(close(fd) == 0);
  assert(close(fd) == -1);

  printf("[double_close_guard] waiting for child confirmation\n");
  assert(wait_for_child(child, NULL) == 0);

  /* Verify the child's write is persisted and the parent can still read it. */
  char merged[CLOSE_BUFFER];
  assert(read_file_into_buffer(CLOSE_FILE, merged, sizeof(merged)) == 0);
  assert(strstr(merged, "guardian-baseline") != NULL);
  assert(strstr(merged, "child-ack-close") != NULL);
  printf("[double_close_guard] verification succeeded\n");

  return 0;
}
