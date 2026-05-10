
#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int execSimple1();
extern int execSimpleHelloworld();
extern int execSimpleMultithread();
extern int execSimplePthreadCreate();
extern int execArgumentsBig();
extern int execArgumentsEmpty();
extern int execSimpleParam();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label) {
  if (rc == 0) {
    printf("EXEC: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("EXEC: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
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
      {execSimple1, "simple exec param check"},
      {execSimpleHelloworld, "exec hello world program"},
      {execSimpleMultithread, "exec with multiple threads, calls exit"},
      {execSimplePthreadCreate, "exec from a pthread, main thread loops"},
      {execSimpleParam, "exec with parameters"},
      {execArgumentsBig, "exec with too big arguments"},
      {execArgumentsEmpty, "exec with empty arguments"},
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
