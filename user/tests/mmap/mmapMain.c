
#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int mmap_file();
extern int mmap_invalid_arg();
extern int mmap_invalid_fd();
extern int mmap_simple_plus();
extern int munmap_partial();

/* Lightweight reporter for each case */
static void report_case(int idx, int rc, int *pass_count, const char *label) {
  if (rc == 0) {
    printf("MMAP: Case %d PASSED - %s\n", idx, label);
    (*pass_count)++;
  } else {
    printf("-------------------------------------------------------------\n");
    printf("MMAP: Case %d FAILED (rc=%d) - %s\n", idx, rc, label);
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
    {mmap_simple_plus, "simple test test"},
    {mmap_file, "basic mmap with file"},
    {mmap_invalid_arg, "invalid arg test"},
    {mmap_invalid_fd, "invalid fd test"},
    {munmap_partial, "munmap partial"},
  };

  int ncases = sizeof(cases) / sizeof(cases[0]);

  for (int i = 0; i < ncases; ++i) {
    total_cases++;
    result = cases[i].fn();
    report_case(total_cases, result, &pass_count, cases[i].desc);
  }

  if (pass_count == total_cases) {
    printf("\n=== MMAP: ALL %d CASES PASSED ===\n", pass_count);
    return 0;
  }

  printf("\n=== MMAP: SOME CASES FAILED (%d/%d) ===\n", pass_count,
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
