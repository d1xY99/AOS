#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

#define FORK 1

extern int brkDecrease();
extern int brkIncrease();
extern int brkInvalidArgs();
extern int sbrkOneWrite();
extern int sbrkZeroWrite();
extern int sbrkSimpleWrite();
extern int sbrkLoop();
extern int sbrkOverwrite();
extern int writeShiftDownWrite();
extern int shiftFiesta();
extern int readAfterFork();
extern int writeAfterFork();
extern int writeAfterExec();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label,
                        int retval) {
  if (rc == retval) {
    printf("BRK: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("BRK: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
    exit(-3);
  }
}

/* Run all test cases declared in the table */
static int execute_cases(void) {
  int pass_count = 0;
  int total_cases = 0;
  int result = 0;
  void *brk_addr = sbrk(0);

  struct test_entry {
    int (*fn)(void);
    const char *desc;
    int retval;
  } cases[] = {
      {sbrkZeroWrite, "sbrk zero write test", 666},
      {brkInvalidArgs, "brk invalid args test", 0},
      {brkDecrease, "brk decrease test", 0},
      {brkIncrease, "brk increase test", 0},
      {sbrkOneWrite, "sbrk one byte write test", 0},
      {sbrkSimpleWrite, "sbrk simple write test", 0},
      {sbrkLoop, "sbrk loop allocation test", 0},
      {sbrkOverwrite, "sbrk overwrite test", 666},
      {writeShiftDownWrite, "write shift down write test", 666},
      {readAfterFork, "read after fork test", 0},
      {writeAfterFork, "write after fork test", 0},
      {writeAfterExec, "write after exec test", 666},
      {shiftFiesta, "shift fiesta multithreaded brk test", 0},
  };

  int ncases = sizeof(cases) / sizeof(cases[0]);

  for (int i = 0; i < ncases; ++i) {
    total_cases++;

    if (!FORK) {
      brk(brk_addr); // reset brk to initial position
      result = cases[i].fn();
      report_case(total_cases, result, &pass_count, cases[i].desc,
                  cases[i].retval);
      continue;
    }

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
      report_case(total_cases, result, &pass_count, cases[i].desc,
                  cases[i].retval);
    }
  }

  if (pass_count == total_cases) {
    printf("\n=== BRK: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== BRK: SOME CASES FAILED (%d/%d) ===\n", pass_count, total_cases);
  return -3;
}

int main(void) {
  int rc = execute_cases();
  if (rc != 0) {
    exit(rc);
  }
  return 0;
}
