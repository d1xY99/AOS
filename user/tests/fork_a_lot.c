#include "sched.h"
#include "unistd.h"

int main() {
  while (1) {
    if (fork() == 0) {
      // Child process
      while (1)
        sched_yield();
    }
  }
  return 0;
}
