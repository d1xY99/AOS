#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int fill_file(int fd, char value, size_t len) {
  char buf[PAGE_SIZE];
  memset(buf, value, sizeof(buf));
  size_t remaining = len;
  while (remaining > 0) {
    size_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);
    ssize_t written = write(fd, buf, chunk);
    if (written != (ssize_t)chunk) {
      return -1;
    }
    remaining -= chunk;
  }
  return 0;
}

int main(void) {
  const char *path = "usr/mmap_shared_visibility.bin";
  const size_t len = PAGE_SIZE;
  int fd = open(path, O_CREAT | O_RDWR, 0600);
  if (fd < 0) {
    printf("open failed\n");
    return 1;
  }

  if (ftruncate(fd, len) != 0) {
    printf("ftruncate failed\n");
    close(fd);
    return 1;
  }

  if (lseek(fd, 0, SEEK_SET) < 0 || fill_file(fd, 'A', len) != 0) {
    printf("init file failed\n");
    close(fd);
    return 1;
  }

  char *view1 = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (view1 == MAP_FAILED) {
    printf("mmap view1 failed\n");
    close(fd);
    return 1;
  }

  char *view2 = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
  if (view2 == MAP_FAILED) {
    printf("mmap view2 failed\n");
    munmap(view1, len);
    close(fd);
    return 1;
  }

  if (view2[0] != 'A') {
    printf("initial shared view mismatch\n");
    munmap(view2, len);
    munmap(view1, len);
    close(fd);
    return 1;
  }

  view1[0] = 'B';
  if (view2[0] != 'B') {
    printf("shared update not visible\n");
    munmap(view2, len);
    munmap(view1, len);
    close(fd);
    return 1;
  }

  munmap(view2, len);
  munmap(view1, len);
  close(fd);
  printf("mmap_shared_visibility passed\n");
  return 0;
}
