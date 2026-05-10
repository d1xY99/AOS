#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "wait.h"

int simple_plus() {
  const char *name = "shm_simple_plus_zone";
  const size_t span = PAGE_SIZE;

  if (shm_open(NULL, O_CREAT | O_RDWR, 0600) >= 0) {
    printf("shm_open null should fail\n");
    return 1;
  }

  if (shm_unlink(NULL) == 0) {
    printf("shm_unlink null should fail\n");
    return 2;
  }

  if (shm_open("missing", O_RDWR, 0600) >= 0) {
    printf("should fail\n");
    return 3;
  }

  int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
  if (fd < 0) {
    printf("shm_open failed\n");
    return 4;
  }

  void *buf = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (buf == MAP_FAILED) {
    printf("mmap failed\n");
    return 5;
  }

  memset(buf, 0, span);
  memcpy(buf, "parent", 7);

  int fd2 = shm_open(name, O_RDWR, 0600);
  if (fd2 < 0) {
    printf("shm_open failed\n");
    return 6;
  }
  
  void *view = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, fd2, 0);
  if (view == MAP_FAILED) {
    printf("second mmap failed\n");
    return 7;
  }

  if (strcmp((const char *)view, "parent") != 0) {
    printf("second mapping mismatch: %s\n", (const char *)view);
    return 8;
  }

  if (shm_unlink(name) != 0) {
    printf("shm_unlink failed\n");
    return 9;
  }

  if (shm_unlink(name) == 0) {
    printf("shm_unlink should fail after unlink\n");
    return 10;
  }

  munmap(view, PAGE_SIZE);
  munmap(buf, span);
  close(fd2);
  close(fd);
  printf("shm_simple_plus passed\n");
  return 0;
}
