#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int writeAfterExec() {
  char *ptr = (char *)sbrk(0);
  sbrk(PAGE_SIZE * 10);

  if (ptr == (char *)-1) {
    return 1;
  }

  printf("Allocated %d bytes at %p\n", PAGE_SIZE * 10, ptr);
  for (int i = 0; i < PAGE_SIZE * 10; i++) {
    ptr[i] = 'B';
  }

  printf("Executing new program...\n");

  if (execv("usr/exec_brk_write.elf", NULL) == -1) {
    printf("execv failed\n");
    return 1;
  }

  assert(0 && "Should not reach this point after execv");

  return 1;
}
