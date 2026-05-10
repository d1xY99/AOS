#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int spinInit();
extern int spinLockUnitialized();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label) {
  if (rc == 0) {
    printf("SPINLOCK: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("SPINLOCK: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
    exit(-3);
  }
}

/* Run all test cases declared in the table */
static int execute_cases(void) {
  int pass_count = 0;
  int total_cases = 0;
  int result = 0;

  struct test_entry {
    int (*fn)(void);
    const char *desc;
  } cases[] = {
      {spinInit, "pthread_spin_init and pthread_spin_destroy"},
      {spinLockUnitialized, "pthread_spin_lock on uninitialized lock"},
  };

  int ncases = sizeof(cases) / sizeof(cases[0]);

  for (int i = 0; i < ncases; ++i) {
    total_cases++;

    // fork and wait for the exec to finish
    size_t pid = fork();
    if (pid == 0) {
      // Child process
      exit(cases[i].fn());
    } else {
      // Parent process
      int status;
      waitpid(pid, &status, 0);
      result = status;
      report_case(total_cases, result, &pass_count, cases[i].desc);
    }
  }

  if (pass_count == total_cases) {
    printf("\n=== SPINLOCK: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== SPINLOCK: SOME CASES FAILED (%d/%d) ===\n", pass_count,
         total_cases);
  return -3;
}

int main(void) {
  int rc = execute_cases();
  if (rc != 0) {
    exit(rc);
  }
  return 0;
}
