#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static int expect_map_failed(void *ptr, const char *label) {
  if (ptr != MAP_FAILED) {
    printf("%s should fail\n", label);
    return 1;
  }
  return 0;
}

int mmap_invalid_fd(void) {
  int fds[2];
  if (pipe(fds) != 0) {
    printf("pipe failed\n");
    return 1;
  }

  int rc = 0;
  rc = expect_map_failed(mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, fds[0], 0),
                          "mmap pipe read fd");
  rc = expect_map_failed(mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE, fds[1], 0),
                          "mmap pipe write fd");

  close(fds[0]);
  close(fds[1]);

  if (rc == 0)
    printf("mmap_invalid_fd passed\n");

  return rc;
}
