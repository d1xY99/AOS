#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int execRecursiveNoEnd() {
  printf("Starting exec_recursive_no_end test...\n");

  if (execv("/usr/exec_recursive_no_end.elf", NULL) == -1) {
    assert(0 && "Should not reach this point\n");
  }

  assert(0 && "Should not reach this point | END\n");
  return 0;
}
