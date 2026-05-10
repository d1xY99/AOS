#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int gp1();
extern int gp2();
extern int accessOtherThreadStack();
extern int demandMapping();
extern int writeIntoStack();
extern int accessOtherThreadStackCreated();
extern int accessOtherThreadStackCreatedDemand();
extern int multiDemand();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label) {
  if (rc == 666 || rc == 9999 || rc == 42) {
    printf("GP: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("GP: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
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
      {gp1, "guard page test 1 - main thread guard page"},
      {gp2, "guard page test 2 - other thread guard page"},
      {writeIntoStack, "write into stack test"},
      {demandMapping, "demand mapping test"},
      {accessOtherThreadStack,
       "access other thread stack if thread not created test"},
      {accessOtherThreadStackCreated, "access other thread stack created test"},
      {accessOtherThreadStackCreatedDemand,
       "access other thread stack created demand mapping test"},
      {multiDemand, "multi demand mapping test"},
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
    printf("\n=== EXEC: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== EXEC: SOME CASES FAILED (%d/%d) ===\n", pass_count,
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
