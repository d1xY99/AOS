set(TEST_DIRECTORIES attr_tests cond pipe_deadlock_suite exec_tests fork_tests guardpage local_fd_tests mutex mutex_fork pipe_tests pthread_cancel_tests pthread_create_tests pthread_join_detach_tests semaphore sleepAndClockTests spinlock)

foreach(dir ${TEST_DIRECTORIES})
  add_dependencies(tests.elf ${dir}.elf)
endforeach()
