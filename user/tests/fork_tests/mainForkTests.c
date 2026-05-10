#include <stdio.h>

int fork_basic_wait(void);
int fork_multiple_wait(void);
int wait_non_child(void);
int nestedFork(void);
int fork_copy_on_write(void);
int fork_wait_any(void);

static int run_test(const char *name, int (*fn)(void))
{
  printf("=== %s ===\n", name);
  int rc = fn();
  if (rc == 0)
    printf("%s: PASS\n\n", name);
  else
    printf("%s: FAIL (rc=%d)\n\n", name, rc);
  return rc;
}

int main(void)
{
  int failures = 0;

  failures += run_test("fork_basic_wait", fork_basic_wait);
  failures += run_test("fork_multiple_wait", fork_multiple_wait);
  failures += run_test("wait_non_child", wait_non_child);
  failures += run_test("nestedFork", nestedFork);
  failures += run_test("fork_copy_on_write", fork_copy_on_write);
  failures += run_test("fork_wait_any", fork_wait_any);

  if (failures == 0)
    printf("All fork tests passed.\n");
  else
    printf("%d fork test(s) failed.\n", failures);

  return failures == 0 ? 0 : 1;
}
