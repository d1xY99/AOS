#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wait.h>

int shmForkShared(void) {
  const char *name = "shm_fork_shared";
  const size_t len = PAGE_SIZE;
  const char *parent_msg = "fork-parent";
  const char *child_msg = "fork-child";
  int rc = 1;
  int fd = -1;
  int created = 0;
  char *view = MAP_FAILED;

  shm_unlink(name);

  fd = shm_open(name, O_CREAT | O_RDWR, 0600);
  if (fd < 0) {
    printf("shm_open failed\n");
    goto out;
  }
  created = 1;

  view = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (view == MAP_FAILED) {
    printf("mmap failed\n");
    goto out;
  }

  memset(view, 0, len);
  memcpy(view, parent_msg, strlen(parent_msg) + 1);

  pid_t pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    goto out;
  }

  if (pid == 0) {
    if (strcmp(view, parent_msg) != 0) {
      printf("child saw '%s' instead of '%s'\n", view, parent_msg);
      _exit(2);
    }
    memcpy(view, child_msg, strlen(child_msg) + 1);
    _exit(0);
  }

  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  if (waited != pid) {
    printf("waitpid returned %d\n", (int)waited);
    goto out;
  }

  if (status != 0) {
    printf("child exited with status %d\n", status);
    goto out;
  }

  if (strcmp(view, child_msg) != 0) {
    printf("parent did not see child update: '%s'\n", view);
    goto out;
  }

  rc = 0;
out:
  if (view != MAP_FAILED)
    munmap(view, len);
  if (fd >= 0)
    close(fd);
  if (created) {
    if (shm_unlink(name) != 0 && rc == 0) {
      printf("shm_unlink failed\n");
      rc = 1;
    }
  }
  return rc;
}
