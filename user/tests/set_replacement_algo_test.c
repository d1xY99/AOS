#include <stdio.h>
#include "nonstd.h"

#define PRA_RANDOM 0
#define PRA_AGING 1
#define PRA_SECOND_CHANCE 2

static int expect_change(size_t pra_value, long expected_rc,
                         const char *label) {
  long rc = (long)set_replacement_algo_test(pra_value);
  if (rc != expected_rc) {
    printf("set_replacement_algo(%s/%zu) returned %ld (expected %ld)\n", label,
           pra_value, rc, expected_rc);
    return -1;
  }

  printf("set_replacement_algo(%s/%zu) succeeded (rc=%ld)\n", label, pra_value, rc);
  return 0;
}

int main(void) {
  int failures = 0;

  if (expect_change(PRA_RANDOM, 0, "RANDOM") != 0) {
    failures++;
  }
  if (expect_change(PRA_AGING, 0, "AGING") != 0) {
    failures++;
  }
  if (expect_change(PRA_SECOND_CHANCE, 0, "SECOND_CHANCE") != 0) {
    failures++;
  }
  if (expect_change(99, -1, "INVALID") != 0) {
    failures++;
  }

  // Restore default PRA to avoid affecting later tests.
  if (expect_change(PRA_RANDOM, 0, "RESTORE_RANDOM") != 0) {
    failures++;
  }

  if (failures) {
    printf("set_replacement_algo_test: %d checks failed\n", failures);
    return -1;
  }

  printf("set_replacement_algo_test: all checks passed\n");
  return 0;
}
