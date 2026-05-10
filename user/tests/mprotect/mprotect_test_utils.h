#pragma once

#include <stdio.h>
#include <unistd.h>
#include <wait.h>

static inline int expect_mprotect_fail(int rc, const char *label) {
  if (rc != -1) {
    printf("%s should fail\n", label);
    return 1;
  }
  return 0;
}

static inline int expect_child_write_fault(char *addr, const char *label) {
  pid_t pid = fork();
  if (pid < 0) {
    printf("%s: fork failed\n", label);
    return 1;
  }
  if (pid == 0) {
    addr[0] ^= 0x5A;
    _exit(0);
  }

  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  if (waited != pid) {
    printf("%s: waitpid failed\n", label);
    return 1;
  }

  if (status == 0) {
    printf("%s: write unexpectedly succeeded\n", label);
    return 1;
  }
  return 0;
}
