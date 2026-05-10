#include "sys/mman.h"
#include "sys/syscall.h"
#include "../../../core/incl/kernel/syscall-definitions.h"

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
void *mmap(void *start, size_t length, int prot, int flags, int fd,
           off_t offset)
{
  if (length == 0)
  {
    return MAP_FAILED;
  }

  const int allowed_prot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_NONE;
  if (prot & ~allowed_prot)
  {
    return MAP_FAILED;
  }

  if ((offset % PAGE_SIZE) != 0)
  {
    return MAP_FAILED;
  }

  if (start && ((size_t)start % PAGE_SIZE))
  {
    return MAP_FAILED;
  }

  if (flags != MAP_PRIVATE && flags != MAP_SHARED &&
      flags != (MAP_PRIVATE | MAP_ANONYMOUS) &&
      flags != (MAP_SHARED | MAP_ANONYMOUS))
  {
    return MAP_FAILED;
  }
  return (void *)__syscall(sc_mmap, length, prot, flags, fd, (size_t)offset);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int munmap(void *start, size_t length)
{
  if (!start || length == 0)
  {
    return -1;
  }
  if ((size_t)start % PAGE_SIZE != 0)
  {
    return -1;
  }
  return (int)__syscall(sc_munmap, (size_t)start, length, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int shm_open(const char *name, int oflag, mode_t mode)
{
  if (!name)
  {
    return -1;
  }
  return (int)__syscall(sc_shm_open, (size_t)name, (size_t)oflag, (size_t)mode,
                        0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int shm_unlink(const char *name)
{
  if (!name)
  {
    return -1;
  }
  return (int)__syscall(sc_shm_unlink, (size_t)name, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int mprotect(void *addr, size_t len, int prot)
{
  if (!addr || len == 0)
  {
    return -1;
  }
  if ((size_t)addr % PAGE_SIZE != 0)
  {
    return -1;
  }

  const int allowed_prot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_NONE;
  if (prot & ~allowed_prot)
  {
    return -1;
  }

  return (int)__syscall(sc_mprotect, (size_t)addr, len, (size_t)prot, 0x0, 0x0);
}
