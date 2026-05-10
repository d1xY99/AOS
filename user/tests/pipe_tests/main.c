#include "pipe_common.h"

static int run_all_tests(void)
{
  struct
  {
    const char *name;
    int (*func)(void);
  } tests[] = {
      {"basic roundtrip", test_roundtrip_single_thread},
      {"threaded roundtrip", test_threaded_roundtrip},
      {"dup keeps pipe alive", test_pipe_duplication},
      {"writer notices reader close", test_pipe_close_propagates},
      {"multi-write reuse", test_pipe_reuse_after_new_writes},
      {"large transfer with blocking", test_pipe_large_transfer},
      {"EOF and EPIPE", test_pipe_eof_and_epipe},
      {"writer blocks until reader consumes", test_pipe_writer_blocks_until_reader_consumes},
      {"reader blocks until writer produces", test_pipe_reader_blocks_until_writer_produces},
  };

  size_t failures = 0;
  for (size_t i = 0; i < PIPE_ARRAY_SIZE(tests); ++i)
  {
    printf("[pipe-tests] RUN %s\n", tests[i].name);
    if (tests[i].func() == 0)
    {
      printf("[pipe-tests] OK  %s\n", tests[i].name);
    }
    else
    {
      ++failures;
      printf("[pipe-tests] ERR %s\n", tests[i].name);
    }
  }

  if (failures)
  {
    printf("[pipe-tests] %zu/%zu tests FAILED\n", failures,
           PIPE_ARRAY_SIZE(tests));
    return 1;
  }

  printf("[pipe-tests] all %zu tests passed\n", PIPE_ARRAY_SIZE(tests));
  return 0;
}

int main(void) { return run_all_tests(); }
