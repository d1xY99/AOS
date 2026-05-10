#include "pthread.h"
#include "assert.h"
#include "sched.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

void wrapper(void *(*start_routine)(void *), void *arg) {
  pthread_exit(start_routine(arg));
}

#define PAGE_SIZE 4096

#define MUTEX_DEBUG_ASSERTS 1

pthread_thread_info *get_pthread_info() {

  int loca_var = 0;
  size_t loca_var_address = (size_t)&loca_var;

  // align to the end (top) of the page
  size_t page_top = loca_var_address | (PAGE_SIZE - 1);

  size_t candidate_addr = page_top + 1 - sizeof(pthread_thread_info);

  // search for the init string in the possible thread stack pages
  for (size_t i = 0; i < MAX_THREAD_STACKS; ++i) {
    pthread_thread_info *candidate =
        (pthread_thread_info *)(candidate_addr + (i * PAGE_SIZE));
    if (memcmp(candidate->init_string, INIT_STRING,
               sizeof(candidate->init_string)) == 0) {
      return candidate;
    }
  }

  assert(0 && "No pthread info found!\n");
  return NULL;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
  return __syscall(sc_pthread_create, (size_t)thread, (size_t)attr,
                   (size_t)start_routine, (size_t)arg, (size_t)wrapper);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_init(pthread_mutex_t *mutex,
                       const pthread_mutexattr_t *attr) {

  memset(mutex, 0, sizeof(pthread_mutex_t));

  pthread_spin_init(&mutex->spinlock, 0);
  mutex->status = PTHREAD_MUTEX_INIT;
  mutex->locked = PTHREAD_MUTEX_UNLOCKED;
  mutex->waiting_threads_head = NULL;
  mutex->waiting_threads_tail = NULL;
  mutex->owner_thread = NULL;
  mutex->type = PTHREAD_MUTEX_ERRORCHECK;

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex) {

  if (mutex->status != PTHREAD_MUTEX_INIT) {
    return -1;
  }

  memset(mutex, 0, sizeof(pthread_mutex_t));

  pthread_spin_destroy(&mutex->spinlock);
  mutex->status = PTHREAD_MUTEX_DESTROYED;
  mutex->locked = PTHREAD_MUTEX_UNLOCKED;
  mutex->waiting_threads_head = NULL;
  mutex->waiting_threads_tail = NULL;
  mutex->owner_thread = NULL;
  mutex->type = PTHREAD_MUTEX_ERRORCHECK;

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_lock(pthread_mutex_t *mutex) {

  if (mutex->status != PTHREAD_MUTEX_INIT) {
    return -1;
  }

  pthread_thread_info *info = get_pthread_info();

  pthread_spin_lock(&mutex->spinlock);

  // already locked
  if (mutex->locked == PTHREAD_MUTEX_LOCKED) {

    // quick self-deadlock check for ERRORCHECK mutexes
    if (mutex->type == PTHREAD_MUTEX_ERRORCHECK &&
        mutex->owner_thread == info) {
      pthread_spin_unlock(&mutex->spinlock);
      return -1;
    }

    // recursive deadlock check
    if (mutex->type == PTHREAD_MUTEX_ERRORCHECK) {
      pthread_mutex_t *current_mutex = mutex;
      pthread_thread_info *current_owner = mutex->owner_thread;
      while (current_mutex != NULL) {
        if (current_owner == info) {
          // deadlock detected
          pthread_spin_unlock(&mutex->spinlock);
          return -1;
        }

        if (current_owner == NULL) {
          break;
        }

        current_mutex = current_owner->waiting_on_mutex;
        if (current_mutex == NULL) {
          break;
        }

        current_owner = current_mutex->owner_thread;
        if (current_owner == NULL) {
          break;
        }
      }
    }

    info->waiting_on_mutex = mutex;

    // add to waiting list
    pthread_thread_info *tail = mutex->waiting_threads_tail;
    pthread_thread_info *head = mutex->waiting_threads_head;
    info->next = NULL;

    if (head == NULL && tail == NULL) {
      // first waiting thread
      mutex->waiting_threads_head = info;
      mutex->waiting_threads_tail = info;
    } else {
      // append to tail
      tail->next = info;
      mutex->waiting_threads_tail = info;
    }

    info->waiting_list_blocked = 1;

    pthread_spin_unlock(&mutex->spinlock);

    if (info->waiting_list_blocked) {
      // this flag lets the scheduler know this thread is waiting on a mutex
      info->block_schedule = 1;
      sched_yield();
    }

    // when we get here, we own the mutex
    pthread_spin_unlock(&mutex->spinlock);

    return 0;
  }

  mutex->locked = PTHREAD_MUTEX_LOCKED;
  mutex->owner_thread = info;
  info->waiting_on_mutex = NULL;

  pthread_spin_unlock(&mutex->spinlock);

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex) {

  if (mutex->status != PTHREAD_MUTEX_INIT) {
    return -1;
  }

  pthread_thread_info *info = get_pthread_info();

  pthread_spin_lock(&mutex->spinlock);

  if (mutex->owner_thread != info) {
    pthread_spin_unlock(&mutex->spinlock);
    // not the owner
    return -1;
  }

  // transfer ownership to next waiting thread
  if (mutex->waiting_threads_head != NULL) {

    pthread_thread_info *next_owner = mutex->waiting_threads_head;

    // pop head
    mutex->waiting_threads_head = next_owner->next;
    if (mutex->waiting_threads_head == NULL) {
      mutex->waiting_threads_tail = NULL;
    }

    next_owner->next = NULL;
    mutex->locked = PTHREAD_MUTEX_LOCKED;
    mutex->owner_thread = next_owner;
    next_owner->waiting_on_mutex = NULL;
    next_owner->waiting_list_blocked = 0;
    next_owner->block_schedule = 0;

    return 0;

  } else {
    mutex->locked = PTHREAD_MUTEX_UNLOCKED;
    mutex->owner_thread = NULL;
  }

  pthread_spin_unlock(&mutex->spinlock);

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
void pthread_exit(void *value_ptr) {
  __syscall(sc_pthread_exit, (size_t)value_ptr, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cancel(pthread_t thread) {
  return __syscall(sc_pthread_cancel, (size_t)thread, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_join(pthread_t thread, void **value_ptr) {
  return __syscall(sc_pthread_join, (size_t)thread, (size_t)(void *)value_ptr,
                   0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_detach(pthread_t thread) {
  return __syscall(sc_pthread_detach, (size_t)thread, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
  (void)attr;

  memset(cond, 0, sizeof(pthread_cond_t));

  cond->status = PTHREAD_COND_INIT;
  cond->waiting_threads_head = NULL;
  cond->waiting_threads_tail = NULL;

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_destroy(pthread_cond_t *cond) {

  if (cond->status != PTHREAD_COND_INIT) {
    return -1;
  }

  memset(cond, 0, sizeof(pthread_cond_t));

  cond->status = PTHREAD_COND_DESTROYED;
  cond->waiting_threads_head = NULL;
  cond->waiting_threads_tail = NULL;

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_signal(pthread_cond_t *cond) {

  if (cond->status != PTHREAD_COND_INIT) {
    return -1;
  }

  pthread_spin_lock(&cond->spinlock);

  // wake up one waiting thread
  if (cond->waiting_threads_head != NULL) {

    pthread_thread_info *next_thread = cond->waiting_threads_head;

    // pop head
    cond->waiting_threads_head = next_thread->next;
    if (cond->waiting_threads_head == NULL) {
      cond->waiting_threads_tail = NULL;
    }

    next_thread->next = NULL;
    next_thread->waiting_list_blocked = 0;
    next_thread->block_schedule = 0;
  }

  pthread_spin_unlock(&cond->spinlock);

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_broadcast(pthread_cond_t *cond) {

  if (cond->status != PTHREAD_COND_INIT) {
    return -1;
  }

  pthread_spin_lock(&cond->spinlock);

  // wake up all waiting threads
  pthread_thread_info *current = cond->waiting_threads_head;
  while (current != NULL) {
    pthread_thread_info *next = current->next;
    current->next = NULL;
    current->waiting_list_blocked = 0;
    current->block_schedule = 0;
    current = next;
  }

  cond->waiting_threads_head = NULL;
  cond->waiting_threads_tail = NULL;

  pthread_spin_unlock(&cond->spinlock);

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {

  if (cond->status != PTHREAD_COND_INIT) {
    return -1;
  }

  pthread_thread_info *info = get_pthread_info();

  // unlock the mutex
  pthread_mutex_unlock(mutex);

  // add to cond wait list
  pthread_spin_lock(&cond->spinlock);
  pthread_thread_info *tail = cond->waiting_threads_tail;
  pthread_thread_info *head = cond->waiting_threads_head;
  info->next = NULL;
  if (head == NULL && tail == NULL) {
    // first waiting thread
    cond->waiting_threads_head = info;
    cond->waiting_threads_tail = info;
  } else {
    // append to tail
    tail->next = info;
    cond->waiting_threads_tail = info;
  }

  info->waiting_list_blocked = 1;
  pthread_spin_unlock(&cond->spinlock);

  if (info->waiting_list_blocked) {
    // this flag lets the scheduler know this thread is waiting on a cond var
    info->block_schedule = 1;
    sched_yield();
  }

  // when we get here, we were signaled
  pthread_mutex_lock(mutex);

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_destroy(pthread_spinlock_t *lock) {

  if (lock->status != SPIN_INIT)
    return -1;

  memset(lock, 0, sizeof(pthread_spinlock_t));

  lock->status = SPIN_DESTROYED;
  lock->lock = SPIN_UNLOCKED;

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_init(pthread_spinlock_t *lock, int pshared) {
  (void)pshared;

  memset(lock, 0, sizeof(pthread_spinlock_t));

  lock->status = SPIN_INIT;
  lock->lock = SPIN_UNLOCKED;

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_lock(pthread_spinlock_t *lock) {

  if (lock->status != SPIN_INIT)
    return -1;

  unsigned int old_val = 1;
  do {
    asm("xchg %0,%1"
        : "=r"(old_val)
        : "m"(lock->lock), "0"(old_val)
        : "memory");
  } while (old_val && !sched_yield());

  return old_val;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_trylock(pthread_spinlock_t *lock) {

  if (lock->status != SPIN_INIT)
    return -1;

  unsigned int old_val = 1;
  asm("xchg %0,%1" : "=r"(old_val) : "m"(lock->lock), "0"(old_val) : "memory");

  if (old_val == 0) {
    return 0; // successfully acquired the lock
  } else {
    return -1; // lock is already held
  }
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_unlock(pthread_spinlock_t *lock) {

  if (lock->status != SPIN_INIT)
    return -1;

  if (lock->lock == SPIN_UNLOCKED)
    return -1;

  lock->lock = SPIN_UNLOCKED;
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_setcancelstate(int state, int *oldstate) {
  return __syscall(sc_pthread_setcancelstate, state, (size_t)oldstate, 0x0, 0x0,
                   0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_setcanceltype(int type, int *oldtype) {
  return __syscall(sc_pthread_setcanceltype, type, (size_t)oldtype, 0x0, 0x0,
                   0x0);
}

int get_thread_count(void) {

  return __syscall(sc_threadcount, 0x0, 0x0, 0x0, 0x0, 0x0);
}

void boot_crash_test(void) { __syscall(sc_boot_crash_test, 0x0, 0x0, 0x0, 0x0, 0x0); }

// get the calling thread's ID without a syscall
int pthread_self(void) { return get_pthread_info()->thread_id; }

int pthread_attr_init(pthread_attr_t *attr) {

  memset(attr, 0, sizeof(pthread_attr_t));
  attr->stack_size = 0;
  attr->status = PTHREAD_ATTR_INIT;

  return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {

  memset(attr, 0, sizeof(pthread_attr_t));
  attr->stack_size = 0;
  attr->status = PTHREAD_ATTR_DESTROYED;

  return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *restrict attr,
                              size_t *restrict stacksize) {

  if (attr->status != PTHREAD_ATTR_INIT) {
    return -1;
  }

  *stacksize = attr->stack_size;

  return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {

  if (attr->status != PTHREAD_ATTR_INIT) {
    return -1;
  }

  if (stacksize < MIN_THREAD_STACKS * PAGE_SIZE ||
      stacksize > MAX_THREAD_STACKS * PAGE_SIZE) {
    return -1;
  }

  attr->stack_size = stacksize;

  return 0;
}
