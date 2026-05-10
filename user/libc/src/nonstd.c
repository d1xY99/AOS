#include "nonstd.h"
#include "sys/syscall.h"
#include "../../../core/incl/kernel/syscall-definitions.h"
#include "stdlib.h"

int createprocess(const char* path, int sleep)
{
  return __syscall(sc_createprocess, (long) path, sleep, 0x00, 0x00, 0x00);
}

int set_replacement_algo_test(size_t pra) {
  return (int)__syscall(set_replacement_algo, pra, 0x0, 0x0, 0x0, 0x0);
}

extern int main();

void _start()
{
  exit(main());
}
