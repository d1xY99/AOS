#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int shmBasic(void) {
  const char *name = "shm_basic_test";
  const size_t len = PAGE_SIZE;
  const char *payload = "hello";

  shm_unlink(name);

  int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
  if (fd < 0) {
    printf("shm_open failed\n");
    return 1;
  }

  char *view = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (view == MAP_FAILED) {
    printf("mmap failed\n");
    close(fd);
    return 1;
  }

  memcpy(view, payload, strlen(payload) + 1);

  if (munmap(view, len) != 0) {
    printf("munmap failed\n");
    close(fd);
    return 1;
  }
  close(fd);

  fd = shm_open(name, O_RDONLY, 0600);
  if (fd < 0) {
    printf("shm_open reopen failed\n");
    return 1;
  }

  const char *read_view = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
  if (read_view == MAP_FAILED) {
    printf("mmap read failed\n");
    close(fd);
    return 1;
  }

  if (strcmp(read_view, payload) != 0) {
    printf("payload mismatch\n");
    munmap((void *)read_view, len);
    close(fd);
    return 1;
  }

  munmap((void *)read_view, len);
  close(fd);

  if (shm_unlink(name) != 0) {
    printf("shm_unlink failed\n");
    return 1;
  }

  return 0;
}
