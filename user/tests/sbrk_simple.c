

#include <stdio.h>
#define PAGE_SIZE 4096

int main() {

  void *ptr = sbrk(PAGE_SIZE);

  if (ptr == (void *)-1) {
    printf("Error: sbrk failed to allocate memory\n");
    return -1;
  }

  printf("sbrk allocated %d bytes at address %p\n", PAGE_SIZE, ptr);

  return 0;
}
