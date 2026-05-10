#include <stdio.h>
#include <stdlib.h>

int clock_progress_test(void);
int sleep_return_test(void);
int usleep_return_test(void);

struct test_case
{
  int (*fn)(void);
  const char *name;
};

static int run_test(const struct test_case *tc, int index)
{
  printf("\n=== [%d] %s ===\n", index, tc->name);
  int rc = tc->fn();
  if (rc == 0)
  {
    printf("[%s] PASS\n", tc->name);
    return 0;
  }

  printf("[%s] FAIL (rc=%d)\n", tc->name, rc);
  return rc;
}

int main(void)
{
  const struct test_case tests[] = {
      {clock_progress_test, "clock() progresses while CPU works"},
      {sleep_return_test, "sleep() completes requested duration"},
      {usleep_return_test, "usleep() completes requested duration"},
  };

  const int count = sizeof(tests) / sizeof(tests[0]);
  int failures = 0;

  for (int i = 0; i < count; ++i)
  {
    if (run_test(&tests[i], i + 1) != 0)
      failures++;
  }

  if (failures == 0)
  {
    printf("\nAll sleep/clock tests passed.\n");
    return 0;
  }

  printf("\nSleep/clock tests failed: %d/%d\n", failures, count);
  return 1;
}
