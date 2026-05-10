#pragma once

#include "../../../core/incl/kernel/kernel_user_shared.h"
#include "../../../core/incl/kernel/syscall-definitions.h"
#include "sys/syscall.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// pthread typedefs
typedef size_t pthread_t;

// pthread mutex typedefs
typedef unsigned int pthread_mutexattr_t;

// pthread cond typedefs
typedef unsigned int pthread_condattr_t;

extern pthread_thread_info *get_pthread_info();

extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine)(void *), void *arg);

extern void pthread_exit(void *value_ptr);

extern int pthread_cancel(pthread_t thread);

extern int pthread_join(pthread_t thread, void **value_ptr);

extern int pthread_detach(pthread_t thread);

extern int pthread_mutex_init(pthread_mutex_t *mutex,
                              const pthread_mutexattr_t *attr);

extern int pthread_mutex_destroy(pthread_mutex_t *mutex);

extern int pthread_mutex_lock(pthread_mutex_t *mutex);

extern int pthread_mutex_unlock(pthread_mutex_t *mutex);

extern int pthread_cond_init(pthread_cond_t *cond,
                             const pthread_condattr_t *attr);

extern int pthread_cond_destroy(pthread_cond_t *cond);

extern int pthread_cond_signal(pthread_cond_t *cond);

extern int pthread_cond_broadcast(pthread_cond_t *cond);

extern int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

extern int pthread_setcancelstate(int state, int *oldstate);

extern int pthread_setcanceltype(int type, int *oldtype);

extern int pthread_spin_destroy(pthread_spinlock_t *lock);

extern int pthread_spin_init(pthread_spinlock_t *lock, int pshared);

extern int pthread_spin_lock(pthread_spinlock_t *lock);

extern int pthread_spin_trylock(pthread_spinlock_t *lock);

extern int pthread_spin_unlock(pthread_spinlock_t *lock);

extern int pthread_attr_getstacksize(const pthread_attr_t *restrict attr,
                                     size_t *restrict stacksize);

extern int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);

extern int pthread_attr_destroy(pthread_attr_t *attr);

extern int pthread_attr_init(pthread_attr_t *attr);

extern int get_thread_count(void);

extern void boot_crash_test(void);

extern int pthread_self(void);

#ifdef __cplusplus
}
#endif
