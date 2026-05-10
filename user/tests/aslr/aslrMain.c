#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int aslrExecBasic();
extern int aslr_exec_runner();
extern int aslrTest();
extern int aslrTest2();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label) {
  if (rc == 0) {
    printf("ASLR: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("ASLR: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
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
      {aslrExecBasic, "exec helper runs"},
      {aslr_exec_runner, "test 3"},
      {aslrTest, "test 4"},
      {aslrTest2, "test 5"},
  };

  int ncases = sizeof(cases) / sizeof(cases[0]);

  for (int i = 0; i < ncases; ++i) {
    total_cases++;
    result = cases[i].fn();
    report_case(total_cases, result, &pass_count, cases[i].desc);
  }

  if (pass_count == total_cases) {
    printf("\n=== ASLR: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== ASLR: SOME CASES FAILED (%d/%d) ===\n", pass_count,
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
