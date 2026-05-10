#include <stdio.h>
#include <unistd.h>

#define SLEEP_MICROSECONDS 10000000 //10 seconds

int main() {
  printf("Starting usleep test...\n");
  printf("Starting to sleep for %d microsecond...\n", SLEEP_MICROSECONDS);
  usleep(SLEEP_MICROSECONDS);
  printf("Woke up after %d microsecond!\n", SLEEP_MICROSECONDS);

  return 0;
}
