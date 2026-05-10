

#include <assert.h>
int execSimpleHelloworld() {

  if (execv("/usr/helloworld.elf", NULL) == -1) {
    assert(0 && "Should not reach this point\n");
    return -1;
  }

  assert(0 && "Should not reach this point\n");
  return 0;
}
