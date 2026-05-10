#include "test_support.h"

#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "wait.h"

#define REOPEN_FILE "usr/reopen_probe.txt"
#define REOPEN_BUFFER 256

int scenario_child_reopen_parent_file(void)
{
  const char *baseline = "baseline-parent\n";
  const char *child_note = "child-reopen-write\n";
  const char *parent_note = "parent-after-child\n";

  printf("[child_reopen_parent_file] preparing baseline content\n");
  assert(reset_file_contents(REOPEN_FILE, baseline) == 0);

  int fd = open(REOPEN_FILE, O_RDWR);
  assert(fd >= 0 && "parent failed to open baseline file");

  pid_t child = fork();
  assert(child >= 0 && "fork failed");

  if (child == 0)
  {
    /* Give the child a clean slate by closing the inherited descriptor. */
    close(fd);

    int reopened = open(REOPEN_FILE, O_RDWR);
    assert(reopened >= 0 && "child failed to reopen shared path");

    char buffer[REOPEN_BUFFER];
    ssize_t read_bytes = read(reopened, buffer, sizeof(buffer) - 1);
    assert(read_bytes >= 0);
    buffer[read_bytes] = '\0';
    assert(strstr(buffer, baseline) != NULL);

    assert(lseek(reopened, 0, SEEK_END) >= 0);
    ssize_t written = write(reopened, child_note, strlen(child_note));
    assert(written == (ssize_t)strlen(child_note));

    close(reopened);
    _exit(0);
  }

  assert(wait_for_child(child, NULL) == 0);

  assert(lseek(fd, 0, SEEK_END) >= 0);
  ssize_t appended = write(fd, parent_note, strlen(parent_note));
  assert(appended == (ssize_t)strlen(parent_note));

  close(fd);

  char verify[REOPEN_BUFFER];
  assert(read_file_into_buffer(REOPEN_FILE, verify, sizeof(verify)) == 0);
  assert(strstr(verify, baseline) != NULL);
  assert(strstr(verify, child_note) != NULL);
  assert(strstr(verify, parent_note) != NULL);
  printf("[child_reopen_parent_file] verification succeeded\n");

  return 0;
}
