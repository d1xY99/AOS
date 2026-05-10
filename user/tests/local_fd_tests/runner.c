#include "assert.h"
#include "stdio.h"

int scenario_shared_read_append(void);
int scenario_multiple_descriptors(void);
int scenario_double_close_guard(void);
int scenario_descriptor_inheritance(void);
int scenario_child_reopen_parent_file(void);
int scenario_descriptor_reuse_after_fork(void);
int scenario_child_open_while_parent_holds_fd(void);
int scenario_child_rejects_postfork_fd(void);

typedef int (*local_fd_test_fn)(void);

typedef struct
{
  const char *description;
  local_fd_test_fn execute;
} local_fd_test_case;

static int run_case(const local_fd_test_case *test, int index)
{
  printf("LocalFD case %d: %s ... ", index + 1, test->description);
  int result = test->execute();
  if (result == 0)
  {
    printf("==============================  ok  ==============================\n");
  }
  else
  {
    printf("/n============================== FAILED (%d)==============================\n", result);
  }
  return result;
}

int main(void)
{
  const local_fd_test_case tests[] = {
      {"child reads baseline and parent sees appended data",
       scenario_shared_read_append},
      {"independent descriptors write unique payloads",
       scenario_multiple_descriptors},
      {"second close fails while child keeps descriptor alive",
       scenario_double_close_guard},
      {"forked descriptors append data sequentially",
       scenario_descriptor_inheritance},
      {"child closes inherited fd then reopens path independently",
       scenario_child_reopen_parent_file},
      {"parent reuses descriptor number while child keeps original file",
       scenario_descriptor_reuse_after_fork},
      {"child opens same path while parent descriptor stays open",
       scenario_child_open_while_parent_holds_fd},
      {"child cannot use parent fd opened after fork",
       scenario_child_rejects_postfork_fd},
  };

  const size_t total = sizeof(tests) / sizeof(tests[0]);
  size_t passed = 0;
  for (size_t i = 0; i < total; ++i)
  {
    if (run_case(&tests[i], (int)i) == 0)
      ++passed;
  }

  printf("LocalFD summary: %zu/%zu passed\n", passed, total);
  assert(passed == total && "LocalFD tests failed");
  return 0;
}
