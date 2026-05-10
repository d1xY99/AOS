#include "unistd.h"

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int brk(void *end_data_segment) {
  return __syscall(sc_brk, (size_t)end_data_segment, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
void *sbrk(intptr_t increment) {
  return (void *)__syscall(sc_sbrk, (size_t)increment, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
unsigned int sleep(unsigned int seconds) {
  return __syscall(sc_sleep, (size_t)seconds, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature
 */
unsigned int usleep(useconds_t microseconds) {
  return __syscall(sc_usleep, (size_t)microseconds, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int ftruncate(int fildes, off_t length) {
  return __syscall(sc_ftruncate, fildes, length, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature
 */
int truncate(const char *path, off_t length) {
  return __syscall(sc_truncate, (size_t)path, length, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature
 */
int access(const char *path, int amode) {
  return __syscall(sc_access, (size_t)path, amode, 0x0, 0x0, 0x0);
}
