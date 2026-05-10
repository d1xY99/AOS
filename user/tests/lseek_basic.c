#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"

#define LSEEK_TEST_FILE "usr/lseek_basic.txt"

int main(void)
{
  const char *seed = "0123456789";
  const size_t seed_len = strlen(seed);

  int fd = open(LSEEK_TEST_FILE, O_RDWR);
  assert(fd >= 0 && "lseek_basic: failed to create test file");

  ssize_t initial = write(fd, seed, seed_len);
  assert(initial == (ssize_t)seed_len && "lseek_basic: failed to seed file");

  size_t pos = lseek(fd, 0, SEEK_SET);
  assert(pos == 0 && "lseek_basic: SEEK_SET to start should return 0");

  pos = lseek(fd, 4, SEEK_SET);
  assert(pos == 4 && "lseek_basic: SEEK_SET should move to requested offset");

  char buffer[4] = {0};
  ssize_t bytes = read(fd, buffer, 2);
  assert(bytes == 2);
  assert(buffer[0] == '4' && buffer[1] == '5');

  pos = lseek(fd, 2, SEEK_CUR);
  assert(pos == 8 && "lseek_basic: SEEK_CUR should advance from current offset");

  pos = lseek(fd, 0, SEEK_END);
  assert(pos == seed_len && "lseek_basic: SEEK_END should report file length");

  const char *suffix = "TAIL";
  ssize_t appended = write(fd, suffix, strlen(suffix));
  assert(appended == (ssize_t)strlen(suffix));

  pos = lseek(fd, 0, SEEK_SET);
  assert(pos == 0);

  char full_buffer[32] = {0};
  bytes = read(fd, full_buffer, sizeof(full_buffer) - 1);
  assert(bytes == (ssize_t)(seed_len + strlen(suffix)));

  const char *expected = "0123456789TAIL";
  assert(strcmp(full_buffer, expected) == 0);

  assert(close(fd) == 0);
  unlink(LSEEK_TEST_FILE);

  printf("[lseek basic] success: %s\n", expected);
  return 0;
}
