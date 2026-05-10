

#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int condInit();
extern int condDestroy();
extern int condNotInitalized();
extern int condSimpleLock();
extern int condBroadcast();
extern int condSignalNoWaiters();
// extern int condLock();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label) {
  if (rc == 0) {
    printf("COND: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("COND: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
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
      {condInit, "cond initialization simple test"},
      {condDestroy, "cond destruction simple test"},
      {condNotInitalized, "cond not initialized test"},
      {condSimpleLock, "cond simple lock/wait/signal test"},
      {condBroadcast, "cond broadcast wake all waiters test"},
      {condSignalNoWaiters, "cond signal with no waiters test"},
  };

  int ncases = sizeof(cases) / sizeof(cases[0]);

  for (int i = 0; i < ncases; ++i) {
    total_cases++;
    result = cases[i].fn();
    report_case(total_cases, result, &pass_count, cases[i].desc);
  }

  if (pass_count == total_cases) {
    printf("\n=== COND: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== COND: SOME CASES FAILED (%d/%d) ===\n", pass_count,
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
