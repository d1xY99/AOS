#include "nonstd.h"
#include <stdio.h>
#include <string.h>

static const char *const k_test_suites[] = {
    "attr_tests",
    //        "cond",
    "pipe_deadlock_suite7", "exec_tests", "fork_tests", "guardpage", "local_fd_tests",
    //       "mutex",
    //      "mutex_fork",
    "pipe_tests", "pthread_cancel_tests", "pthread_create_tests",
    "pthread_join_detach_tests",
    //     "semaphore",
    "sleepAndClockTests", "spinlock"};

static int run_suite(const char *suite_name) {
  char path[64];
  const char prefix[] = "/usr/";
  size_t prefix_len = sizeof(prefix) - 1;
  size_t suite_len = strlen(suite_name);
  const char suffix[] = ".elf";
  size_t suffix_len = sizeof(suffix) - 1;

  size_t total = prefix_len + suite_len + suffix_len + 1;
  if (total > sizeof(path)) {
    printf("[tests] path truncated for %s\n", suite_name);
    return -1;
  }

  memcpy(path, prefix, prefix_len);
  memcpy(path + prefix_len, suite_name, suite_len);
  memcpy(path + prefix_len + suite_len, suffix, suffix_len);
  path[prefix_len + suite_len + suffix_len] = '\0';

  printf("\n[tests] Running %s...\n", suite_name);
  int rc = createprocess(path, 1);
  if (rc != 0)
    printf("[tests] %s failed with rc=%d\n", suite_name, rc);
  else
    printf("[tests] %s completed successfully\n", suite_name);
  return rc;
}

int main(void) {
  size_t failures = 0;
  size_t total = sizeof(k_test_suites) / sizeof(k_test_suites[0]);

  for (size_t i = 0; i < total; ++i) {
    if (run_suite(k_test_suites[i]) != 0)
      ++failures;
  }

  printf("\n[tests] Summary: %zu/%zu suites failed\n", failures, total);
  return failures == 0 ? 0 : 1;
}
