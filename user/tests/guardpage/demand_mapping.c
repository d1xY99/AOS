#include "unistd.h"
#include <assert.h>
#include <stdio.h>

#define PAGE_SIZE 4096

int demandMapping() {

  printf("Test demand mapping\n");

  int local_var = 42;
  char *ptr = (char *)&local_var;

  size_t counter = 0;

  // write one page and then wait
  while (1) {

    *ptr = 'A';
    ptr--;
    counter++;

    if (counter % PAGE_SIZE == 0) {
      printf("Written %zu bytes towards lower addresses\n", counter);
      sleep(1);
    }
  }

  assert(0 && "Should never reach this point");

  return 0;
}
