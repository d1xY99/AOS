

#include <assert.h>
#include <stdio.h>
int execSimple1() {

  printf("Starting exec_simple_1 test...\n");

  if (execv("non_existent_file", NULL) == -1) {
    printf("execv failed as expected for non-existent file\n");
    return 0;
  }

  assert(0 && "Should not reach this point\n");
  return 0;
}
