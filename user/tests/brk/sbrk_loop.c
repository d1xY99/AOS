#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define N_PAGES 10

int sbrkLoop() {
  void *start = sbrk(0);
  printf("start brk=%p\n", start);
  for (int i = 0; i < N_PAGES; ++i) {
    void *prev = sbrk(PAGE_SIZE);
    if (prev == (void *)-1) {
      printf("sbrk failed at iteration %d\n", i);
      return 1;
    }
    char *last_page = (char *)start + (i * PAGE_SIZE);
    last_page[0] = (char)i;
  }
  printf("allocated %d pages. brk now=%p\n", N_PAGES, sbrk(0));
  return 0;
}
