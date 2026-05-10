#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main() {
  const size_t page = PAGE_SIZE;

  if (mmap(NULL, page, PROT_READ, 0, -1, 0) != MAP_FAILED) {
    printf("mmap invalid flags should fail\n");
    return 1;
  }

  if (mmap(NULL, 0, PROT_READ, MAP_PRIVATE, -1, 0) != MAP_FAILED) {
    printf("mmap zero length should fail\n");
    return 2;
  }

  if (mmap(NULL, page, PROT_READ, MAP_SHARED, -1, 1) != MAP_FAILED) {
    printf("mmap unaligned offset should fail\n");
    return 3;
  }

  if (mmap((void *)1, page, PROT_READ, MAP_SHARED, -1, 0) != MAP_FAILED) {
    printf("mmap unaligned address should fail\n");
    return 4;
  }

  // anonymous mapping should start zeroed
  void *anon = mmap(NULL, page, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (anon == MAP_FAILED) {
    printf("mmap anonymous failed\n");
    return 5;
  }

  if (((const char *)anon)[0] != 0 || ((const char *)anon)[page - 1] != 0) {
    printf("anonymous page not zeroed\n");
    return 6;
  }
  munmap(anon, page);

  printf("mmap_simple_plus passed\n");
  return 0;
}
