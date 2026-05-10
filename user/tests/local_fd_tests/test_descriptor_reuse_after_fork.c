#include "test_support.h"

#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "wait.h"

#define REUSE_OLD_FILE "usr/reuse_old.txt"
#define REUSE_NEW_FILE "usr/reuse_new.txt"
#define REUSE_BUFFER 256

int scenario_descriptor_reuse_after_fork(void)
{
  const char *old_seed = "old-file-original\n";
  const char *new_seed = "new-file-original\n";
  const char *child_suffix = "child-old-append\n";
  const char *parent_suffix = "parent-new-append\n";

  printf("[descriptor_reuse_after_fork] preparing files\n");
  assert(reset_file_contents(REUSE_OLD_FILE, old_seed) == 0);
  assert(reset_file_contents(REUSE_NEW_FILE, new_seed) == 0);

  int fd = open(REUSE_OLD_FILE, O_RDWR);
  assert(fd >= 0 && "parent failed to open old file");

  pid_t child = fork();
  assert(child >= 0 && "fork failed");

  if (child == 0)
  {
    /* Let the parent close and reuse the descriptor number first. */
    sleep(1);

    char buffer[REUSE_BUFFER];
    ssize_t read_bytes = read(fd, buffer, sizeof(buffer) - 1);
    assert(read_bytes >= 0);
    buffer[read_bytes] = '\0';
    assert(strstr(buffer, old_seed) != NULL);

    assert(lseek(fd, 0, SEEK_END) >= 0);
    ssize_t written = write(fd, child_suffix, strlen(child_suffix));
    assert(written == (ssize_t)strlen(child_suffix));

    close(fd);
    _exit(0);
  }

  assert(close(fd) == 0);

  int reused_fd = open(REUSE_NEW_FILE, O_RDWR);
  assert(reused_fd >= 0 && "parent failed to open new file");

  if (reused_fd != fd)
  {
    printf("[descriptor_reuse_after_fork] descriptor number changed "
           "(old %d, new %d)\n", fd, reused_fd);
  }

  assert(lseek(reused_fd, 0, SEEK_END) >= 0);
  ssize_t appended = write(reused_fd, parent_suffix, strlen(parent_suffix));
  assert(appended == (ssize_t)strlen(parent_suffix));

  close(reused_fd);

  assert(wait_for_child(child, NULL) == 0);

  char old_contents[REUSE_BUFFER];
  char new_contents[REUSE_BUFFER];
  assert(read_file_into_buffer(REUSE_OLD_FILE, old_contents, sizeof(old_contents)) == 0);
  assert(read_file_into_buffer(REUSE_NEW_FILE, new_contents, sizeof(new_contents)) == 0);

  assert(strstr(old_contents, child_suffix) != NULL);
  assert(strstr(old_contents, parent_suffix) == NULL);

  assert(strstr(new_contents, parent_suffix) != NULL);
  assert(strstr(new_contents, child_suffix) == NULL);

  printf("[descriptor_reuse_after_fork] verification succeeded\n");
  return 0;
}
