#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

int shmReopenShared(void) {
  const char *name = "shm_reopen_shared";
  const size_t len = PAGE_SIZE;

  shm_unlink(name);

  int fd1 = shm_open(name, O_CREAT | O_RDWR, 0600);
  if (fd1 < 0) {
    printf("shm_open fd1 failed\n");
    return 1;
  }

  int fd2 = shm_open(name, O_RDWR, 0600);
  if (fd2 < 0) {
    printf("shm_open fd2 failed\n");
    close(fd1);
    return 1;
  }

  char *view1 = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);
  if (view1 == MAP_FAILED) {
    printf("mmap view1 failed\n");
    close(fd2);
    close(fd1);
    return 1;
  }

  char *view2 = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
  if (view2 == MAP_FAILED) {
    printf("mmap view2 failed\n");
    munmap(view1, len);
    close(fd2);
    close(fd1);
    return 1;
  }

  view1[0] = 'X';
  if (view2[0] != 'X') {
    printf("shared update not visible\n");
    munmap(view2, len);
    munmap(view1, len);
    close(fd2);
    close(fd1);
    return 1;
  }

  view2[0] = 'Y';
  if (view1[0] != 'Y') {
    printf("shared update not visible\n");
    munmap(view2, len);
    munmap(view1, len);
    close(fd2);
    close(fd1);
    return 1;
  }

  munmap(view2, len);
  munmap(view1, len);
  close(fd2);
  close(fd1);

  if (shm_unlink(name) != 0) {
    printf("shm_unlink failed\n");
    return 1;
  }

  return 0;
}
