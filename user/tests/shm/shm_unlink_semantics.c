#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int shmUnlinkSemantics(void) {
  const char *name = "shm_unlink_semantics";
  const size_t len = PAGE_SIZE;

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

  view[0] = 'u';
  view[1] = 'r';
  view[2] = ' ';
  view[3] = 'a';
  view[4] = ' ';
  view[5] = 'w';
  view[6] = 'i';
  view[7] = 'z';
  view[8] = 'a';
  view[9] = 'r';
  view[10] = 'd';
  view[11] = ' ';
  view[12] = 'h';
  view[13] = 'a';
  view[14] = 'r';
  view[15] = 'r';
  view[16] = 'y';
  view[17] = '\0';

  if (shm_unlink(name) != 0) {
    printf("shm_unlink failed\n");
    munmap(view, len);
    close(fd);
    return 1;
  }

  if (shm_open(name, O_RDWR, 0600) >= 0) {
    printf("shm_open after unlink should fail\n");
    munmap(view, len);
    close(fd);
    return 1;
  }

  if (strcmp(view, "ur a wizard harry") != 0) {
    printf("mapping not preserved after unlink\n");
    munmap(view, len);
    close(fd);
    return 1;
  }

  munmap(view, len);
  close(fd);

  if (shm_unlink(name) == 0) {
    printf("shm_unlink should fail after unlink\n");
    return 1;
  }

  return 0;
}
