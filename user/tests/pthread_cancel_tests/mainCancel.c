#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

extern int cancel_async_other();
extern int cancel_self_immediate();
extern int cancel_pending_enable();
extern int cancel_invalid_target();
extern int cancel_disable_ignored();
extern int cancel_multiple_requests();

/* Report outcome for one test case and update counters */
static void report_result(int id, int rc, int *succeeded, const char *msg)
{
  if (rc == 0)
  {

    printf("CANCEL: Case %d SUCCEEDED — %s\n", id, msg);
    printf("-------------------------------------------------------------\n\n");

    (*succeeded)++;
    return;
  }

  printf("CANCEL: Case %d FAILED (code=%d) — %s\n", id, rc, msg);
  printf("-------------------------------------------------------------\n\n");

  exit(-3);
}

/* Execute the suite of pthread_cancel tests */
static int execute_suite(void)
{
  int passed = 0;
  int total = 0;
  int rc;

  struct test_def
  {
    int (*fn)(void);
    const char *description;
  } suite[] = {
      {cancel_async_other, "asynchronous cancel of another thread"},
      {cancel_self_immediate, "self-cancel exits immediately"},
      {cancel_pending_enable, "pending cancel delivered after enabling and switching type"},
      {cancel_invalid_target, "cancel returns ESRCH for non-existent or exited threads"},
      {cancel_disable_ignored, "cancel request ignored while thread keeps cancellation disabled"},
      {cancel_multiple_requests, "duplicate cancel requests still succeed and cancel once"},
  };

  int n = sizeof(suite) / sizeof(suite[0]);

  for (int i = 0; i < n; ++i)
  {
    total++;
    rc = suite[i].fn();
    report_result(total, rc, &passed, suite[i].description);
  }

  if (passed == total)
  {
    printf("\n=== pthread_cancel: ALL %d TESTS PASSED ===\n", passed);
    return 0;
  }

  printf("\n=== pthread_cancel: SOME TESTS FAILED (%d/%d) ===\n", (total - passed), total);
  return -3;
}

int main(void)
{
  int rc = execute_suite();
  if (rc != 0)
  {
    exit(rc);
  }
  return 0;
}
