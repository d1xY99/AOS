#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int expect_map_failed(void *ptr, const char *label) {
  if (ptr != MAP_FAILED) {
    printf("%s should fail\n", label);
    return 1;
  }
  return 0;
}

static int expect_minus_one(int rc, const char *label) {
  if (rc != -1) {
    printf("%s should fail\n", label);
    return 1;
  }
  return 0;
}

int mmap_invalid_arg(void) {
  int rc = 0;

  void *p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (p == MAP_FAILED) {
    printf("valid mmap failed\n");
    return 1;
  }

  memset(p, 0xA5, PAGE_SIZE);
  if (munmap(p, PAGE_SIZE) != 0) {
    printf("valid munmap failed\n");
    return 1;
  }

  rc |= expect_map_failed(mmap(NULL, 0, PROT_READ,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0),
                          "mmap(length=0)");

  rc |= expect_map_failed(mmap(NULL, PAGE_SIZE, PROT_READ,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 1),
                          "mmap(unaligned offset)");

  rc |= expect_map_failed(mmap(NULL, PAGE_SIZE, 0x8,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0),
                          "mmap(invalid prot)");

  rc |= expect_map_failed(mmap(NULL, PAGE_SIZE, PROT_READ,
                               MAP_PRIVATE | MAP_SHARED, -1, 0),
                          "mmap(invalid flags)");

  rc |= expect_map_failed(mmap(NULL, PAGE_SIZE, PROT_READ,
                               MAP_SHARED, -1, 0),
                          "mmap(shared without fd)");

  rc |= expect_minus_one(munmap(NULL, PAGE_SIZE), "munmap(NULL)");
  rc |= expect_minus_one(munmap((void *)1, PAGE_SIZE), "munmap(unaligned)");
  rc |= expect_minus_one(munmap((void *)PAGE_SIZE, 0), "munmap(length=0)");

  if (rc == 0) {
    printf("mmap_invalid_args passed\n");
  }

  return rc;
}
