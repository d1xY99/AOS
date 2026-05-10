#include "test_support.h"

#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "wait.h"

#define SHARED_OPEN_FILE "usr/shared_open_probe.txt"
#define SHARED_OPEN_BUFFER 256

int scenario_child_open_while_parent_holds_fd(void)
{
  const char *seed = "shared-open-baseline\n";
  const char *child_fragment = "child-open-fragment\n";
  const char *parent_fragment = "parent-post-child\n";

  printf("[child_open_while_parent_holds_fd] preparing file\n");
  assert(reset_file_contents(SHARED_OPEN_FILE, seed) == 0);

  int parent_fd = open(SHARED_OPEN_FILE, O_RDWR);
  assert(parent_fd >= 0 && "parent failed to open shared file");

  pid_t child = fork();
  assert(child >= 0 && "fork failed");

  if (child == 0)
  {
    int child_fd = open(SHARED_OPEN_FILE, O_RDWR);
    assert(child_fd >= 0 && "child failed to open file while parent still holds descriptor");

    char baseline[SHARED_OPEN_BUFFER];
    ssize_t read_bytes = read(child_fd, baseline, sizeof(baseline) - 1);
    assert(read_bytes >= 0);
    baseline[read_bytes] = '\0';
    assert(strstr(baseline, seed) != NULL);

    assert(lseek(child_fd, 0, SEEK_END) >= 0);
    ssize_t written = write(child_fd, child_fragment, strlen(child_fragment));
    assert(written == (ssize_t)strlen(child_fragment));

    close(child_fd);
    close(parent_fd); /* close inherited copy */
    _exit(0);
  }

  assert(wait_for_child(child, NULL) == 0);

  assert(lseek(parent_fd, 0, SEEK_END) >= 0);
  ssize_t appended = write(parent_fd, parent_fragment, strlen(parent_fragment));
  assert(appended == (ssize_t)strlen(parent_fragment));

  close(parent_fd);

  char combined[SHARED_OPEN_BUFFER];
  assert(read_file_into_buffer(SHARED_OPEN_FILE, combined, sizeof(combined)) == 0);
  assert(strstr(combined, seed) != NULL);
  assert(strstr(combined, child_fragment) != NULL);
  assert(strstr(combined, parent_fragment) != NULL);
  printf("[child_open_while_parent_holds_fd] verification succeeded\n");

  return 0;
}
