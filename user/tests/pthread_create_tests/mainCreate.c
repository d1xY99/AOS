#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int create_invalid_inputs();
extern int create_single_join();
extern int create_argument_roundtrip();
extern int create_wave_stress();

/* Evaluate a single test case and report outcome */
static void evaluate_case(int case_id, int result_code, int *successes, const char *note)
{
  if (result_code == 0)
  {

    printf(">>> pthread_create TEST %d: OK\n", case_id);
    printf("Info: %s\n", note);
    printf("-------------------------------------------------\n\n");
    (*successes)++;
    return;
  }

  printf(">>> pthread_create TEST %d: ERROR (code=%d)\n", case_id, result_code);
  printf("Info: %s\n", note);
  printf("-------------------------------------------------\n\n");
  exit(-3);
}

/* Run the full pthread_create test collection */
static int execute_tests(void)
{
  int case_num = 0;
  int successes = 0;
  int outcome;

  struct case_entry
  {
    int (*fn)(void);
    const char *description;
  } suite[] = {
      {create_invalid_inputs, "reject invalid pointers and missing start functions"},
      {create_single_join, "create a single thread and collect its return value"},
      {create_argument_roundtrip, "verify argument propagation across many threads"},
      {create_wave_stress, "launch threads in waves to ensure unique ids and cleanup"},
  };

  int total = sizeof(suite) / sizeof(suite[0]);

  for (int i = 0; i < total; ++i)
  {
    case_num++;
    outcome = suite[i].fn();
    evaluate_case(case_num, outcome, &successes, suite[i].description);
  }

  if (case_num == successes)
  {
    printf("\n*** pthread_create: ALL %d TESTS PASSED ***\n", successes);
    return 0;
  }

  printf("\n*** pthread_create: FAILURES (%d/%d) ***\n", (case_num - successes), case_num);
  return -3;
}

int main(void)
{
  int rc = execute_tests();
  if (rc != 0)
    exit(rc);
  return 0;
}
