#pragma once

#define fd_stdin 0
#define fd_stdout 1
#define fd_stderr 2

#define sc_exit 1
#define sc_fork 2
#define sc_read 3
#define sc_write 4
#define sc_open 5
#define sc_waitpid 7
#define sc_close 6
#define sc_lseek 19
#define sc_pseudols 43
#define sc_outline 105
#define sc_sched_yield 158
#define sc_createprocess 191
#define sc_trace 252

#define sc_boot_crash_test 420
#define sc_threadcount 421

// phtread sycalls start here 10000+
#define sc_pthread_create 10001
#define sc_pthread_join 10002
#define sc_pthread_cancel 10003
#define sc_pthread_exit 10004
#define sc_pthread_detach 10005
#define sc_pthread_setcancelstate 10006
#define sc_pthread_setcanceltype 10007
#define sc_pthread_self 10008

// other sycalls start here 20000+
#define sc_sleep 20001
#define sc_clock 20002
#define sc_pipe 20003
#define sc_dup 20004
#define sc_dup2 20005
#define sc_usleep 20006
#define sc_rename 20007
#define sc_ftruncate 20008
#define sc_truncate 20009
#define sc_access 20010
#define sc_execv 20011

// Heap
#define sc_brk 30000
#define sc_sbrk 30001
#define sc_mmap 30002
#define sc_munmap 30003
#define sc_shm_open 30004
#define sc_shm_unlink 30005
#define sc_mprotect 30006

#define set_replacement_algo 40000
