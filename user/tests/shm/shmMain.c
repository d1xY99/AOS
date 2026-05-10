
#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int shmBasic();
extern int shmReopenShared();
extern int simple_plus();
extern int shmForkShared();
extern int shmExecShared();
extern int shmInvalidFlags();
extern int shmUnlinkSemantics();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label) {
  if (rc == 0) {
    printf("SHM: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("SHM: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
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
    {shmBasic, "basic shm test"},
    {simple_plus, "simple plus shm test"},
    {shmReopenShared, "shared updates across two mappings"},
    {shmForkShared, "shared updates survive fork"},
    {shmExecShared, "shared updates survive exec"},
    {shmInvalidFlags, "invalid flags test"},
    {shmUnlinkSemantics, "unlinking test"},
  };

  int ncases = sizeof(cases) / sizeof(cases[0]);

  for (int i = 0; i < ncases; ++i) {
    total_cases++;
    result = cases[i].fn();
    report_case(total_cases, result, &pass_count, cases[i].desc);
  }

  if (pass_count == total_cases) {
    printf("\n=== SHM: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== SHM: SOME CASES FAILED (%d/%d) ===\n", pass_count,
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
