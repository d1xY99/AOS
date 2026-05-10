#include "test_support.h"

#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "wait.h"

#define INHERITED_FILE "usr/sequence_chain.txt"
#define INHERITED_BUFFER 256

int scenario_descriptor_inheritance(void)
{
  printf("[descriptor_inheritance] seeding base file\n");
  /* Seed the file with short message so offset handling is obvious. */
  const char *seed = "seed-start\n";
  assert(reset_file_contents(INHERITED_FILE, seed) == 0);

  int fd = open(INHERITED_FILE, O_RDWR);
  assert(fd >= 0);

  /* Consume existing content so the shared offset sits at end-of-file. */
  char drain[INHERITED_BUFFER];
  while (read(fd, drain, sizeof(drain)) > 0)
  {
    /* intentionally drain to reach EOF */
  }

  pid_t child = fork();
  assert(child >= 0);

  if (child == 0)
  {
    printf("[descriptor_inheritance:child] appending child fragment\n");
    /* Child writes from inherited offset; we expect append immediately after seed. */
    const char *child_fragment = "branch-child\n";
    ssize_t out = write(fd, child_fragment, strlen(child_fragment));
    assert(out == (ssize_t)strlen(child_fragment));

    close(fd);
    _exit(0);
  }

  assert(wait_for_child(child, NULL) == 0);

  /* Parent writes an additional fragment after the child to check ordering. */
  printf("[descriptor_inheritance:parent] appending parent fragment\n");
  const char *parent_fragment = "root-parent\n";
  ssize_t written = write(fd, parent_fragment, strlen(parent_fragment));
  assert(written == (ssize_t)strlen(parent_fragment));

  assert(close(fd) == 0);

  char combined[INHERITED_BUFFER];
  if (read_file_into_buffer(INHERITED_FILE, combined, sizeof(combined)) != 0)
  {
    assert(0 && "failed to read descriptor inheritance file");
  }

  /* Expect fragments appended sequentially without stray bytes. */
  const char *expected_sequence = "seed-start\nbranch-child\nroot-parent\n";
  size_t expected_len = strlen(expected_sequence);
  printf("[descriptor_inheritance] Combined content:\n%s", combined);
  printf("[descriptor_inheritance] Expected content:\n%s", expected_sequence);
  printf("[descriptor_inheritance] Expected length: %zu, Actual length: %zu\n",
         expected_len, strlen(combined));
  assert(strlen(combined) == expected_len);
  for (size_t i = 0; i < expected_len; ++i)
  {
    assert(combined[i] == expected_sequence[i]);
  }
  printf("[descriptor_inheritance] verification succeeded\n");

  return 0;
}
