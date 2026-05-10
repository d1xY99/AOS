#pragma once

#include "CleanupThread.h"
#include "IdleThread.h"
#include "SwappingThread.h"
#include "types.h"
#include <ulist.h>

#define DEFAULT_FREQUENCY 18 // 18 Hz
#define IRQ0_TIME 55         // 55 ms

class Thread;
class Mutex;
class SpinLock;
class Lock;

class Scheduler {
public:
  static Scheduler *instance();

  void addNewThread(Thread *thread);
  void sleep();
  void wake(Thread *thread_to_wake);
  void yield();
  void printThreadList();
  void printStackTraces();
  void printLockingInformation();
  bool isSchedulingEnabled();
  bool isCurrentlyCleaningUp();
  void incTicks();
  void calculateCPUFrequency();
  size_t getTicks();
  uint64 readTimeStampCounter();
  size_t getRandomNumber(size_t max_value);
  size_t getRandomNumberInRange(size_t min_value, size_t max_value);

  /**
   * NEVER EVER EVER CALL THIS METHOD OUTSIDE OF AN INTERRUPT CONTEXT
   * this is the method that decides which threads will be scheduled next
   * it is called by either the timer interrupt handler or the yield interrupt
   * handler and changes the global variables currentThread and
   * currentThreadRegisters
   */
  void schedule();

  /**
   * returns the number of threads currently registered with the scheduler
   */
  uint32 getThreadCount();

  uint64 cpu_frequency_;
  uint64 cycles_per_tick_;

  bool isCpuFrequencyReady() const;

  char *getUpTimeString();

protected:
  friend class IdleThread;
  friend class CleanupThread;
  friend class SwappingThread;

  void cleanupDeadThreads();

private:
  Scheduler();

  /**
   * Scheduler internal lock abstraction method
   * locks the thread-list against concurrent access by prohibiting a thread
   * switch don't call this from an Interrupt-Handler, since Atomicity won't be
   * guaranteed
   */
  void lockScheduling();

  /**
   * Scheduler internal lock abstraction method
   * unlocks the thread-list
   */
  void unlockScheduling();

  static Scheduler *instance_;

  typedef aostl::list<Thread *> ThreadList;
  ThreadList threads_;

  size_t block_scheduling_;

  size_t ticks_;

  size_t unfinished_cleanup_counter_;

  IdleThread idle_thread_;
  CleanupThread cleanup_thread_;
  SwappingThread swapping_thread_;

  uint64 first_timestamp_counter_;
  size_t last_calibration_tick_;
  bool cycles_per_tick_calibrated_;
};
