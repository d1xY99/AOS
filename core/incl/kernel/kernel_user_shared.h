#include "types.h"
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define USER_BREAK 0x0000800000000000ULL
#define KERNEL_START 0xffff800000000000ULL
#define GUARD_PAGE 1
#define PRIME_NUMBER 27211
#define INIT_STRING "INITIALIZED"

#define MIN_SHARED_MEM_ADDRESS (USER_BREAK / 2)
#define MAX_SHARED_MEM_ADDRESS (USER_BREAK - (USER_BREAK / 8))

#define MIN_SHARED_MEM_VPN (MIN_SHARED_MEM_ADDRESS / PAGE_SIZE)
#define MAX_SHARED_MEM_VPN (MAX_SHARED_MEM_ADDRESS / PAGE_SIZE)

#define PROT_NONE 0x00000000
#define PROT_READ 0x00000001
#define PROT_WRITE 0x00000002
#define PROT_EXEC 0x00000004

#define MAP_PRIVATE 0x20000000
#define MAP_SHARED 0x40000000
#define MAP_ANONYMOUS 0x80000000

#define MAP_FAILED ((void *)-1)

#define HIGHEST_USER_VPN (USER_BREAK / PAGE_SIZE - 1)

#define THREAD_STACK_ASLR 0
#define MAX_STACK_ASLR_OFFSET USER_BREAK / PAGE_SIZE / 2

#define HEAP_START USER_BREAK / 4
#define HEAP_ASLR 1
#define MAX_HEAP_ASLR_OFFSET (MIN_SHARED_MEM_VPN - (HEAP_START / PAGE_SIZE) - 1)

#define SWAP_ENABLED 1
#define PRE_SWAP_ENABLED 0
#define PRE_SWAPPING_FREE_PAGE_THRESHOLD 56

#define SHM_ASLR 1
#define SHM_ASLR_ATTEMPTS 8

// for threads
#define PTHREAD_CANCELED -1

// #define PTHREAD_CANCEL_ENABLE 0
// #define PTHREAD_CANCEL_DISABLE 1
//
// #define PTHREAD_CANCEL_DEFERRED 0
// #define PTHREAD_CANCEL_ASYNCHRONOUS 1

typedef struct pthread_mutex_t pthread_mutex_t;
typedef struct sem_t sem_t;
typedef struct pthread_cond_t pthread_cond_t;
typedef struct pthread_attr_t pthread_attr_t;
typedef struct pthread_spinlock_t pthread_spinlock_t;

// pthread spinlock typedefs
#define SPIN_LOCKED 1
#define SPIN_UNLOCKED 0
#define SPIN_DESTROYED 2
#define SPIN_INIT 3

typedef struct pthread_spinlock_t {
  unsigned int status;
  unsigned int lock;
} pthread_spinlock_t;

// thread info
typedef struct pthread_thread_info {
  void *tls_self;
  void *tls_data_base;
  size_t tls_data_size;
  size_t thread_id;
  int block_schedule;
  int waiting_list_blocked;
  struct pthread_thread_info *next;
  pthread_mutex_t *waiting_on_mutex;
  sem_t *waiting_on_semaphore;
  char init_string[12];
} pthread_thread_info;

// pthread mutex typedefs
#define PTHREAD_MUTEX_LOCKED 1
#define PTHREAD_MUTEX_UNLOCKED 0

#define PTHREAD_MUTEX_DESTROYED 1
#define PTHREAD_MUTEX_INIT 2

#define PTHREAD_MUTEX_NORMAL 0

// Robusteness -> either | Relock -> error | Unlock when not owner -> error
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE 2

typedef unsigned int pthread_mutexattr_t;
struct pthread_mutex_t {
  unsigned int status;
  unsigned int locked;
  unsigned int type;
  pthread_thread_info *owner_thread;
  pthread_thread_info *waiting_threads_head;
  pthread_thread_info *waiting_threads_tail;
  pthread_spinlock_t spinlock;
};

// semaphores typedefs

#define SEM_DESTROYED 1
#define SEM_INIT 2

struct sem_t {
  unsigned int status;
  unsigned int lock_value;
  pthread_thread_info *waiting_threads_head;
  pthread_thread_info *waiting_threads_tail;
  pthread_spinlock_t spinlock;
};

// condition variables typedefs

#define PTHREAD_COND_DESTROYED 1
#define PTHREAD_COND_INIT 2

typedef struct pthread_cond_t {
  unsigned int status;
  pthread_thread_info *waiting_threads_head;
  pthread_thread_info *waiting_threads_tail;
  pthread_spinlock_t spinlock;
} pthread_cond_t;

// pthread attributes typedefs

#define PTHREAD_ATTR_INIT 1
#define PTHREAD_ATTR_DESTROYED 2

typedef struct pthread_attr_t {
  unsigned int status;
  size_t stack_size;
} pthread_attr_t;

// set stack size for threads
#define MAX_THREAD_STACKS 8
#define MIN_THREAD_STACKS 1

#ifdef __cplusplus
}
#endif
