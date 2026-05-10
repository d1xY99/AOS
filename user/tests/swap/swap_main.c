
#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

#define FORK_AND_EXEC 0

extern int moreBrk();
extern int swapWriteRead();
extern int swapWriteReadTwice();
extern int swapEveryFourth();
extern int swapTwoRegions();
extern int swapLarge();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label) {
  if (rc == 0) {
    printf("SWAP: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("SWAP: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
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
      //{moreBrk, "Testing if swapping out works a lot work with brk"},//goes too long
      {swapWriteRead, "Stride over pages and verify contents"},
      {swapWriteReadTwice, "Touch pages in two passes"},
      {swapEveryFourth, "Sparse page touches"},
      {swapTwoRegions, "Alternate between two regions"},
      {swapLarge, "Large allocation and verify"},
  };

  int ncases = sizeof(cases) / sizeof(cases[0]);

  for (int i = 0; i < ncases; ++i) {
    total_cases++;

    if (!FORK_AND_EXEC) {
      result = cases[i].fn();
      report_case(total_cases, result, &pass_count, cases[i].desc);
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
      report_case(total_cases, result, &pass_count, cases[i].desc);
    }
  }

  if (pass_count == total_cases) {
    printf("\n=== SWAP: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== SWAP: SOME CASES FAILED (%d/%d) ===\n", pass_count,
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
