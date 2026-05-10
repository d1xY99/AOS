#include <stdio.h>
#include <unistd.h>

#define SLEEP_SECONDS 2

int main() {
  printf("Starting sleep_simple_1 test...\n");
  printf("Starting to sleep for %d second...\n", SLEEP_SECONDS);
  sleep(SLEEP_SECONDS);
  printf("Woke up after %d second!\n", SLEEP_SECONDS);

  return 0;
}
