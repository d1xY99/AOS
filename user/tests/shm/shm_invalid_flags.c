#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>

static int expect_fail(int rc, const char *label) {
  if (rc >= 0) {
    printf("%s should fail\n", label);
    return 1;
  }
  return 0;
}

int shmInvalidFlags(void) {
  int rc = 0;

  rc = expect_fail(shm_open(NULL, O_CREAT | O_RDWR, 0600), "shm_open(NULL)");
  rc = expect_fail(shm_open("missing_obj", O_RDWR, 0600), "shm_open missing");
  rc = expect_fail(shm_open("excl_no_create", O_EXCL | O_RDWR, 0600), "shm_open O_EXCL without O_CREAT");
  rc = expect_fail(shm_unlink(NULL), "shm_unlink(NULL)");

  if (rc == 0)
    printf("shm_invalid_flags passed\n");

  return rc;
}
