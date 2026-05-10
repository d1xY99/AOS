#include <stdio.h>
// #include <stdint.h>

static int global_counter = 1;

static int sample_function(void)
{
  return 0x1234;
}

int main(void)
{
  puts("[non_writable_code] verifying data segment is writable...");
  global_counter += 41;
  printf("[non_writable_code] global_counter=%d (expect 42)\n", global_counter);

  puts("[non_writable_code] attempting to modify code page (expect fault)...");
  volatile unsigned char *func = (unsigned char *)&sample_function;
  *func = 0x90; // Should trigger a fault if code is read-only

  puts("[non_writable_code] ERROR: write unexpectedly succeeded!\n");
  return 1;
}
