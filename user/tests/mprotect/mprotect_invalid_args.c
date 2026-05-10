#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mprotect_test_utils.h"

int mprotect_invalid_args(void) {
  int rc = 0;
  void *region = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (region == MAP_FAILED) {
    printf("mmap failed\n");
    return 1;
  }

  rc |= expect_mprotect_fail(mprotect(NULL, PAGE_SIZE, PROT_READ),
                             "mprotect(NULL)");

  rc |= expect_mprotect_fail(mprotect((char *)region + 1, PAGE_SIZE, PROT_READ),
                             "mprotect(unaligned addr)");

  rc |= expect_mprotect_fail(mprotect(region, 0, PROT_READ),
                             "mprotect(len=0)");

  rc |= expect_mprotect_fail(mprotect(region, PAGE_SIZE, 0x8000),
                             "mprotect(invalid prot bits)");

  if (munmap(region, PAGE_SIZE) != 0) {
    printf("munmap failed\n");
    return 1;
  }

  rc |= expect_mprotect_fail(mprotect(region, PAGE_SIZE, PROT_READ),
                             "mprotect(unmapped)");

  if (rc == 0) {
    printf("mprotect_invalid_args passed\n");
  }
  return rc;
}
