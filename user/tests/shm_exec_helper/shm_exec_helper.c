#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc < 4) {
    printf("usage: shm_exec_helper <name> <expected> <new_value>\n");
    return 1;
  }

  const char *name = argv[1];
  const char *expected = argv[2];
  const char *new_value = argv[3];

  int fd = shm_open(name, O_RDWR, 0600);
  if (fd < 0) {
    printf("helper shm_open failed\n");
    return 2;
  }

  char *view = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (view == MAP_FAILED) {
    printf("helper mmap failed\n");
    close(fd);
    return 3;
  }

  if (strcmp(view, expected) != 0) {
    printf("helper expected '%s' but saw '%s'\n", expected, view);
    munmap(view, PAGE_SIZE);
    close(fd);
    return 4;
  }

  size_t new_len = strlen(new_value) + 1;
  if (new_len > PAGE_SIZE) {
    printf("helper new value too large\n");
    munmap(view, PAGE_SIZE);
    close(fd);
    return 5;
  }

  memset(view, 0, PAGE_SIZE);
  memcpy(view, new_value, new_len);

  munmap(view, PAGE_SIZE);
  close(fd);
  return 0;
}
