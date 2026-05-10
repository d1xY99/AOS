#pragma once

#include <types.h>
#include <utypes.h>

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

class Syscall {
public:
  static size_t syscallException(size_t syscall_number, size_t arg1,
                                 size_t arg2, size_t arg3, size_t arg4,
                                 size_t arg5);

  static void exit(size_t exit_code);
  static void outline(size_t port, pointer text);

  static size_t write(size_t fd, pointer buffer, size_t size);
  static size_t read(size_t fd, pointer buffer, size_t count);
  static size_t lseek(size_t fd, l_off_t offset, size_t whence);
  static size_t close(size_t fd);
  static size_t open(size_t path, size_t flags);
  static void pseudols(const char *pathname, char *buffer, size_t size);
  static int pipe(int *file_descriptor_array);
  static int dup(int file_descriptor_to_copy_id);
  static int dup2(int old_fd, int new_fd);
  static size_t rename(size_t old_path_ptr, size_t new_path_ptr);
  static int access(size_t path_ptr, int mode);

  static int ftruncate(int fd, off_t length);
  static int truncate(size_t path, off_t length);

  static uint32 get_thread_count();

  static void boot_crash_test();

  static size_t sleep(size_t seconds);
  static size_t usleep(size_t micorseconds);

  static size_t pthread_create(size_t *thread, size_t pthread_attr_t,
                               void *(*start_routine)(void *), void *arg,
                               size_t wrapper);

  static size_t clock();

  static size_t createprocess(size_t path, size_t sleep);
  static size_t fork();
  static size_t waitpid(size_t pid, int *status, int options);
  static void trace();

  static void handle_pending_cancel(const char *location);

  static size_t execv(const char *path, char *const argv[]);

  static void pthread_exit(void *value_ptr);
  static size_t pthread_cancel(size_t thread);
  static size_t pthread_detach(size_t thread);
  static size_t pthread_setcancelstate(int state, int *oldstate);
  static size_t pthread_setcanceltype(int type, int *oldtype);
  static size_t pthread_join(size_t thread, void **value_ptr);
  static size_t pthread_self();

  static size_t set_replacement_algo_func(size_t pra);

  static size_t brk(void *addr);
  static size_t sbrk(size_t increment);

  static void *mmap(void *start, size_t length, int prot, int flags, int fd,
                    off_t offset);
  static int munmap(void *start, size_t length);
  static int mprotect(void *addr, size_t len, int prot);
  static int shm_open(const char *name, int oflag, mode_t mode);
  static int shm_unlink(const char *name);
};
