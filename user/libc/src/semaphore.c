#include "semaphore.h"
#include "pthread.h"
#include "sched.h"
#include "string.h"

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_wait(sem_t *sem) {

  if (sem->status != SEM_INIT) {
    return -1;
  }

  pthread_thread_info *info = get_pthread_info();

  pthread_spin_lock(&sem->spinlock);

  // slow path - need to wait
  if (sem->lock_value == 0) {
    // add to waiting list
    pthread_thread_info *tail = sem->waiting_threads_tail;
    pthread_thread_info *head = sem->waiting_threads_head;

    info->next = NULL;
    info->waiting_on_semaphore = sem;

    if (head == NULL && tail == NULL) {
      // first thread in the waiting list
      sem->waiting_threads_head = info;
      sem->waiting_threads_tail = info;
    } else {
      tail->next = info;
      sem->waiting_threads_tail = info;
    }

    info->waiting_list_blocked = 1;

    pthread_spin_unlock(&sem->spinlock);

    if (info->waiting_list_blocked) {
      // this flag lets the scheduler know this thread is waiting on a mutex
      info->block_schedule = 1;
      sched_yield();
    }

    pthread_spin_unlock(&sem->spinlock);

    // when we get scheduled again, we have the semaphore
    return 0;
  }

  sem->lock_value--;
  pthread_spin_unlock(&sem->spinlock);
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_trywait(sem_t *sem) {

  if (sem->status != SEM_INIT) {
    return -1;
  }

  pthread_spin_lock(&sem->spinlock);

  if (sem->lock_value == 0) {
    pthread_spin_unlock(&sem->spinlock);
    return -1;
  }

  sem->lock_value--;
  pthread_spin_unlock(&sem->spinlock);
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_init(sem_t *sem, int pshared, unsigned value) {
  (void)pshared;

  memset(sem, 0, sizeof(sem_t));

  pthread_spin_init(&sem->spinlock, 0);
  sem->status = SEM_INIT;
  sem->lock_value = value;
  sem->waiting_threads_head = NULL;
  sem->waiting_threads_tail = NULL;

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_destroy(sem_t *sem) {

  if (sem->status != SEM_INIT) {
    return -1;
  }

  memset(sem, 0, sizeof(sem_t));

  pthread_spin_destroy(&sem->spinlock);
  sem->status = SEM_DESTROYED;
  sem->lock_value = 0;
  sem->waiting_threads_head = NULL;
  sem->waiting_threads_tail = NULL;

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_post(sem_t *sem) {

  if (sem->status != SEM_INIT) {
    return -1;
  }

  pthread_spin_lock(&sem->spinlock);

  // transfer semaphore to next waiting thread
  if (sem->waiting_threads_head != NULL) {

    pthread_thread_info *next_owner = sem->waiting_threads_head;

    // pop head
    sem->waiting_threads_head = next_owner->next;
    if (sem->waiting_threads_head == NULL) {
      sem->waiting_threads_tail = NULL;
    }

    next_owner->next = NULL;
    next_owner->waiting_on_semaphore = NULL;
    next_owner->waiting_list_blocked = 0;
    next_owner->block_schedule = 0;

    return 0;
  } else {
    sem->lock_value++;
  }

  pthread_spin_unlock(&sem->spinlock);
  return 0;
}
