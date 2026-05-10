

#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int mutexInit();
extern int mutexInitDouble();
extern int mutexInitDestroy();
extern int mutexNotInitialized();
extern int mutexLockSimple();
extern int mutexLock();
extern int mutexLockMulti();
extern int mutexLockNoWork();
extern int mutexDeadlock();
extern int mutexRelock();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label) {
  if (rc == 0) {
    printf("MUTEX: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("MUTEX: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
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
      {mutexInit, "mutex initialization simple test"},
      {mutexInitDouble, "mutex initialization double init test"},
      {mutexInitDestroy, "mutex initialization and destroy test"},
      {mutexNotInitialized, "mutex not initialized test"},
      {mutexLockSimple, "mutex initialization simple test"},
      {mutexRelock, "mutex relock test"},
      {mutexLock, "mutex lock/unlock with multiple threads test"},
      {mutexLockMulti, "mutex multi-lock/unlock with multiple threads test"},
      {mutexLockNoWork, "mutex lock/unlock with no work test"},
      {mutexDeadlock, "mutex deadlock test"},
  };

  int ncases = sizeof(cases) / sizeof(cases[0]);

  for (int i = 0; i < ncases; ++i) {
    total_cases++;
    result = cases[i].fn();
    report_case(total_cases, result, &pass_count, cases[i].desc);
  }

  if (pass_count == total_cases) {
    printf("\n=== MUTEX: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== MUTEX: SOME CASES FAILED (%d/%d) ===\n", pass_count,
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
