#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mprotect_test_utils.h"

int mprotect_split_range(void) {
  int rc = 0;
  size_t len = PAGE_SIZE * 2;
  char *buf = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (buf == MAP_FAILED) {
    printf("mmap failed\n");
    return 1;
  }

  memset(buf, 0x11, PAGE_SIZE);
  memset(buf + PAGE_SIZE, 0x22, PAGE_SIZE);

  if (mprotect(buf + PAGE_SIZE, PAGE_SIZE, PROT_READ) != 0) {
    printf("mprotect on second page failed\n");
    munmap(buf, len);
    return 1;
  }

  buf[0] = 0x33;
  if (buf[0] != 0x33) {
    printf("write to first page failed\n");
    munmap(buf, len);
    return 1;
  }

  rc |= expect_child_write_fault(buf + PAGE_SIZE, "write on protected page");

  if (mprotect(buf + PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE) != 0) {
    printf("mprotect restore on second page failed\n");
    munmap(buf, len);
    return 1;
  }

  buf[PAGE_SIZE] = 0x44;
  if (buf[PAGE_SIZE] != 0x44) {
    printf("write after restore failed\n");
    munmap(buf, len);
    return 1;
  }

  if (munmap(buf, len) != 0) {
    printf("munmap failed\n");
    return 1;
  }

  if (rc == 0) {
    printf("mprotect_split_range passed\n");
  }
  return rc;
}
