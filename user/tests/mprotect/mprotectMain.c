
#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int mprotect_invalid_args(void);
extern int mprotect_permissions(void);
extern int mprotect_split_range(void);

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label)
{
  if (rc == 0)
  {
    printf("MPROTECT: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  }
  else
  {
    printf("-------------------------------------------------------------\n");
    printf("MPROTECT: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
    exit(-3);
  }
}

/* Run all test cases declared in the table */
static int execute_cases(void)
{
  int pass_count = 0;
  int total_cases = 0;
  int result = 0;

  struct test_entry
  {
    int (*fn)(void);
    const char *desc;
  } cases[] = {
      {mprotect_invalid_args, "invalid arguments rejected"},
      // {mprotect_permissions, "permission changes and enforcement"},
      {mprotect_split_range, "partial-range protection updates"},
  };

  int ncases = sizeof(cases) / sizeof(cases[0]);

  for (int i = 0; i < ncases; ++i)
  {
    total_cases++;
    result = cases[i].fn();
    report_case(total_cases, result, &pass_count, cases[i].desc);
  }

  if (pass_count == total_cases)
  {
    printf("\n=== MPROTECT: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== MPROTECT: SOME CASES FAILED (%d/%d) ===\n", pass_count,
         total_cases);
  return -3;
}

int main(void)
{
  int rc = execute_cases();
  if (rc != 0)
  {
    exit(rc);
  }
  return 0;
}
