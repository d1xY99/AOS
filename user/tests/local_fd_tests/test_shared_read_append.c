#include "test_support.h"

#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "wait.h"

#define SHARED_FILE "usr/coauthor_notes.txt"
#define SHARED_BUFFER 256

int scenario_shared_read_append(void)
{
  printf("[shared_read_append] preparing baseline content\n");
  /* Reset file to known content so both processes read the same baseline. */
  const char *initial_line = "parent-intro-line\n";
  assert(reset_file_contents(SHARED_FILE, initial_line) == 0);

  /* Parent opens the file read/write once, expecting fork to inherit it. */
  int fd = open(SHARED_FILE, O_RDWR);
  assert(fd >= 0 && "parent failed to open shared file");

  pid_t child = fork();
  assert(child >= 0 && "fork should succeed");

  if (child == 0)
  {
    printf("[shared_read_append:child] reading baseline and appending follow-up\n");
    char child_buffer[SHARED_BUFFER];

    /* Child reads immediately; inherited offset is at start, expect baseline. */
    ssize_t read_bytes = read(fd, child_buffer, sizeof(child_buffer) - 1);
    size_t baseline_len = strlen(initial_line);
    assert(read_bytes >= (ssize_t)baseline_len);
    child_buffer[read_bytes] = '\0';
    char saved_char = child_buffer[baseline_len];
    child_buffer[baseline_len] = '\0';
    assert(strcmp(child_buffer, initial_line) == 0);
    child_buffer[baseline_len] = saved_char;

    /* Child appends new data that the parent will later verify. */
    const char *child_note = "child-followup-line\n";
    ssize_t written = write(fd, child_note, strlen(child_note));
    assert(written == (ssize_t)strlen(child_note));

    close(fd);
    _exit(0);
  }

  /* Parent waits so the child finishes its write before verification. */
  printf("[shared_read_append:parent] waiting for child to finish\n");
  assert(wait_for_child(child, NULL) == 0);

  /* Parent re-opens read-only to ensure it observes child writes from disk. */
  close(fd);
  fd = open(SHARED_FILE, O_RDONLY);
  assert(fd >= 0 && "parent reopen for verification failed");

  char parent_buffer[SHARED_BUFFER];
  ssize_t total_read = read(fd, parent_buffer, sizeof(parent_buffer) - 1);
  assert(total_read > 0);
  parent_buffer[total_read] = '\0';

  /* Expect both baseline and appended line to appear in order. */
  assert(strstr(parent_buffer, initial_line) != NULL);
  assert(strstr(parent_buffer, "child-followup-line") != NULL);

  close(fd);
  printf("[shared_read_append] verification succeeded\n");
  return 0;
}
