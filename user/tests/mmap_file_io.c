#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int verify_pattern(const char *buf, size_t len, char value) {
  for (size_t i = 0; i < len; ++i) {
    if (buf[i] != value) {
      return 1;
    }
  }
  return 0;
}

int main(void) {
  const char *path = "usr/mmap_file_io_test.bin";
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

  char *shared = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shared == MAP_FAILED) {
    printf("mmap shared failed\n");
    close(fd);
    return 1;
  }
  memset(shared, 'A', len);
  if (munmap(shared, len) != 0) {
    printf("munmap shared failed\n");
    close(fd);
    return 1;
  }

  if (lseek(fd, 0, SEEK_SET) < 0) {
    printf("lseek failed\n");
    close(fd);
    return 1;
  }

  char read_back[PAGE_SIZE];
  ssize_t got = read(fd, read_back, len);

  if (got != (ssize_t)len) {
    printf("read back failed\n");
    close(fd);
    return 1;
  }

  if (verify_pattern(read_back, len, 'A') != 0) {
    printf("MAP_SHARED did not persist to file\n");
    close(fd);
    return 1;
  }

  char *priv = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (priv == MAP_FAILED) {
    printf("mmap private failed\n");
    close(fd);
    return 1;
  }

  memset(priv, 'B', len);
  if (munmap(priv, len) != 0) {
    printf("munmap private failed\n");
    close(fd);
    return 1;
  }

  if (lseek(fd, 0, SEEK_SET) < 0) {
    printf("lseek failed\n");
    close(fd);
    return 1;
  }

  got = read(fd, read_back, len);
  if (got != (ssize_t)len) {
    printf("read back failed\n");
    close(fd);
    return 1;
  }

  if (verify_pattern(read_back, len, 'A') != 0) {
    printf("MAP_PRIVATE should not modify file\n");
    close(fd);
    return 1;
  }

  close(fd);
  printf("mmap_file_io passed\n");
  return 0;
}
