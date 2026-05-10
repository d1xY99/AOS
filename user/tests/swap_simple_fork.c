#include "wait.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int main(void) {
  const int pages = 256; // moderate size to trigger swap
  char *start = (char *)sbrk(0);
  if (start == (char *)-1) {
    printf("sbrk\n");
    return 1;
  }
  if (sbrk((size_t)pages * PAGE_SIZE) == (void *)-1) {
    printf("sbrk grow\n");
    return 1;
  }

  for (int i = 0; i < pages; ++i)
    start[i * PAGE_SIZE] = 'P';

  pid_t pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    return 2;
  }

  return 0;
}
