#include "time.h"

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
clock_t clock(void) {
  return (clock_t)__syscall(sc_clock, 0x0, 0x0, 0x0, 0x0, 0x0);
}
