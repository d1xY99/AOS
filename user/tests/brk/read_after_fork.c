
#include "stdio.h"
#include "unistd.h"

#define PAGE_SIZE 4096

int readAfterFork() {

  void *ptr = sbrk(0);
  sbrk(PAGE_SIZE);
  if (ptr == (void *)-1) {
    return 1;
  }

  // Write to the newly allocated memory to ensure the page is mapped
  char *data = (char *)ptr;
  for (size_t i = 0; i < PAGE_SIZE; i++) {
    data[i] = 'A';
  }

  printf("Parent wrote to allocated memory.\n");

  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    printf("Child process reading allocated memory:\n");
    char *child_data = (char *)ptr;
    for (size_t i = 0; i < PAGE_SIZE; i++) {
      if (i % 1000 == 0) {
        printf("%c\n", child_data[i]);
      }
      if (child_data[i] != 'A') {
        return -1;
      }
    }
  } else {
    waitpid(pid, NULL, 0);
  }

  return 0;
}
