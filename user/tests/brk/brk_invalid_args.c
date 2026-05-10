#include "../../../core/incl/kernel/kernel_user_shared.h"
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

int brkInvalidArgs() {
  void *orig = sbrk(0);
  if (orig == (void *)-1) {
    printf("sbrk(0) failed\n");
    return 666;
  }

  if (brk(NULL) != -1) {
    printf("brk(NULL) should fail\n");
    return 666;
  }

  if (brk((void *)(PAGE_SIZE - 1)) != -1) {
    printf("brk(PAGE_SIZE - 1) should fail\n");
    return 666;
  }

  if (brk((void *)USER_BREAK) != -1) {
    printf("brk(USER_BREAK) should fail\n");
    return 666;
  }

  if (sbrk(0) != orig) {
    printf("brk failure should not move break\n");
    return 666;
  }

  return 0;
}
