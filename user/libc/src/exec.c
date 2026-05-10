#include "../../../core/incl/kernel/syscall-definitions.h"
#include "stdarg.h"
#include "stdlib.h"
#include "sys/syscall.h"
#include "unistd.h"

/**
 * Creates a child process.
 * The new process will be nearly identical to the callee except for its
 * PID and PPID.
 * Resource utilizations are set to zero, file locks are not inherited.
 *
 */
pid_t fork() { return __syscall(sc_fork, 0x00, 0x00, 0x00, 0x00, 0x00); }

/**
 * Replaces the current process image with a new one.
 * The values provided with the argv array are the arguments for the new
 * image starting with the filename of the file to be executed (by convention).
 * This pointer array must be terminated by a NULL pointer (which has to be
 * of type char *.
 * If this function returns, an error has occured.
 *
 * @param path path to the file to execute
 * @param argv an array containing the arguments
 * @return 0 on success, -1 otherwise and errno is set appropriately
 *
 */
int execv(const char *path __attribute__((unused)),
          char *const argv[] __attribute__((unused))) {

  return __syscall(sc_execv, (size_t)path, (size_t)argv, 0x00, 0x00, 0x00);
}

/**
 * Terminates the calling process. Any open file descriptors belonging to the
 * process are closed, any children of the process are inherited by process
 * 1, init, and the process's parent is sent a SIGCHLD signal.
 * The value status is returned to the parent process as the process's exit
 * status and can be collected using a wait call.
 * This function does NOT call any functions registered with atexit(), nor any
 * registered signal handlers.
 *
 * @param status exit status of the process
 *
 */
void _exit(int status) { __syscall(sc_exit, status, 0x00, 0x00, 0x00, 0x00); }
void exit(int status) { __syscall(sc_exit, status, 0x00, 0x00, 0x00, 0x00); }
