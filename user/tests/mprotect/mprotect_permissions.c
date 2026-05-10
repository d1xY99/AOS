#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mprotect_test_utils.h"

int mprotect_permissions(void)
{
  printf("mprotect_permissions: verifying read permissions\n");
  int rc = 0;
  char *buf = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (buf == MAP_FAILED)
  {
    printf("mmap failed\n");
    return 1;
  }
  printf("mprotect_permissions: verifying read permissions\n");

  buf[0] = 0x11;
  buf[PAGE_SIZE - 1] = 0x22;

  if (mprotect(buf, PAGE_SIZE, PROT_NONE) != 0)
  {
    printf("mprotect(PROT_NONE) failed\n");
    munmap(buf, PAGE_SIZE);
    return 1;
  }

  if (mprotect(buf, PAGE_SIZE, PROT_READ) != 0)
  {
    printf("mprotect(PROT_READ) failed\n");
    munmap(buf, PAGE_SIZE);
    return 1;
  }
  printf("mprotect_permissions: verifying read permissions\n");
  if (buf[0] != 0x11 || buf[PAGE_SIZE - 1] != 0x22)
  {
    printf("data changed across mprotect round-trip\n");
    munmap(buf, PAGE_SIZE);
    return 1;
  }

  rc |= expect_child_write_fault(buf, "write on read-only page");

  if (mprotect(buf, PAGE_SIZE, PROT_READ | PROT_WRITE) != 0)
  {
    printf("mprotect(PROT_READ|PROT_WRITE) failed\n");
    munmap(buf, PAGE_SIZE);
    return 1;
  }

  buf[0] = 0xAA;
  buf[PAGE_SIZE - 1] = 0xBB;
  if (buf[0] != 0xAA || buf[PAGE_SIZE - 1] != 0xBB)
  {
    printf("write after restoring permissions failed\n");
    munmap(buf, PAGE_SIZE);
    return 1;
  }

  if (munmap(buf, PAGE_SIZE) != 0)
  {
    printf("munmap failed\n");
    return 1;
  }

  if (rc == 0)
  {
    printf("mprotect_permissions passed\n");
  }
  return rc;
}
