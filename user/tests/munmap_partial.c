#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

static int expect_zero(int rc, const char *label) {
  if (rc != 0) {
    printf("%s failed\n", label);
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

int main(void) {
  int rc = 0;
  size_t len = PAGE_SIZE * 2;

  char *base = mmap(NULL, len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED) {
    printf("mmap failed\n");
    return 1;
  }

  memset(base, 0x11, PAGE_SIZE);
  memset(base + PAGE_SIZE, 0x22, PAGE_SIZE);

  rc = expect_zero(munmap(base + PAGE_SIZE, PAGE_SIZE),
                    "munmap(second page)");

  rc = expect_minus_one(munmap(base + PAGE_SIZE, PAGE_SIZE * 2),
                         "munmap(range with hole)");

  rc = expect_zero(munmap(base, PAGE_SIZE), "munmap(first page)");

  rc = expect_minus_one(munmap(base, PAGE_SIZE), "munmap(double free)");

  if (rc == 0) {
    printf("munmap_partial passed\n");
  }
  return rc;
}
