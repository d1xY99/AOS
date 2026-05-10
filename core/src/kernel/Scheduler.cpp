#include "Scheduler.h"
#include "ArchCommon.h"
#include "ArchInterrupts.h"
#include "ArchThreads.h"
#include "KernelHeap.h"
#include "Lock.h"
#include "Mutex.h"
#include "Syscall.h"
#include "Thread.h"
#include "UserProcess.h"
#include "UserThread.h"
#include "backtrace.h"
#include "debug.h"
#include "kprintf.h"
#include "panic.h"
#include "kernel_user_shared.h"
#include "types.h"
#include "umap.h"
#include "ustring.h"
#include <ulist.h>

ArchThreadRegisters *currentThreadRegisters;
Thread *currentThread;

Scheduler *Scheduler::instance_ = nullptr;

namespace {
constexpr size_t CPU_CALIBRATION_TICKS = 64;
constexpr size_t CPU_CALIBRATION_WARMUP = 4;
} // namespace

Scheduler *Scheduler::instance() {
  if (unlikely(!instance_))
    instance_ = new Scheduler();
  return instance_;
}

Scheduler::Scheduler() {
  block_scheduling_ = 0;
  ticks_ = 0;
  unfinished_cleanup_counter_ = 0;
  cpu_frequency_ = 0;
  cycles_per_tick_ = 0;
  first_timestamp_counter_ = readTimeStampCounter();
  last_calibration_tick_ = 0;
  cycles_per_tick_calibrated_ = false;
  addNewThread(&cleanup_thread_);
  addNewThread(&idle_thread_);
  addNewThread(&swapping_thread_);
}

void Scheduler::schedule() {
  assert(!ArchInterrupts::testIFSet() &&
         "Tried to schedule with Interrupts enabled");
  if (block_scheduling_) {
    debug(SCHEDULER, "schedule: currently blocked\n");
    return;
  }

  if (currentThread == &cleanup_thread_ && unfinished_cleanup_counter_ > 5) {
    debug(SCHEDULER, "schedule: WARNING - cleanup_thread is being descheduled "
                     "before completing cleanup.\n");
  }

  if (currentThread && currentThread->type_ == Thread::TYPE::USER_THREAD &&
      currentThread->getState() == Running &&
      ((UserThread *)currentThread)->process_)
    ((UserThread *)currentThread)->process_->clockOutTimestamp();

  auto it = threads_.begin();
  for (; it != threads_.end(); ++it) {

    auto next_thread = *it;

    // if the current thread is a userthread and has no process, kill it
    // we continue here because the thread already got marked as ToBeDestroyed
    // in the UserThread::kill function
    if (next_thread->type_ == Thread::TYPE::USER_THREAD) {
      UserThread *ut = (UserThread *)(*it);
      if (!ut->process_) {
        continue;
      }
    }

    if (next_thread->schedulable()) {
      currentThread = *it;
      break;
    }
  }

  assert(it != threads_.end() && "No schedulable thread found");
  aostl::rotate(threads_.begin(), it + 1,
               threads_.end()); // no new/delete here - important because
                                // interrupts are disabled

  currentThreadRegisters = currentThread->switch_to_userspace_
                               ? currentThread->user_registers_
                               : currentThread->kernel_registers_;

  if (currentThread && currentThread->type_ == Thread::TYPE::USER_THREAD &&
      currentThread->getState() == Running &&
      ((UserThread *)currentThread)->process_)
    ((UserThread *)currentThread)->process_->clockInTimestamp();
}

void Scheduler::addNewThread(Thread *thread) {
  assert(thread);
  debug(SCHEDULER, "addNewThread: %p  %zd:%s\n", thread, thread->getTID(),
        thread->getName());
  if (currentThread)
    ArchThreads::debugCheckNewThread(thread);
  KernelHeap::instance()->getKMMLock().acquire();
  lockScheduling();
  KernelHeap::instance()->getKMMLock().release();
  threads_.push_back(thread);
  unlockScheduling();
}

void Scheduler::sleep() {
  currentThread->setState(Sleeping);
  assert(block_scheduling_ == 0);
  yield();
}

void Scheduler::wake(Thread *thread_to_wake) {
  // wait until the thread is sleeping
  while (thread_to_wake->getState() != Sleeping)
    yield();
  thread_to_wake->setState(Running);
}

void Scheduler::yield() {
  assert(this);
  if (!ArchInterrupts::testIFSet()) {
    assert(currentThread);
    kprintfd("Scheduler::yield: WARNING Interrupts disabled, do you really "
             "want to yield ? (currentThread %p %s)\n",
             currentThread, currentThread->name_.c_str());
    currentThread->printBacktrace();
  }
  ArchThreads::yield();
}

void Scheduler::cleanupDeadThreads() {
  /* Before adding new functionality to this function, consider if that
     functionality could be implemented more cleanly in another place.
     (e.g. Thread/Process destructor) */

  assert(currentThread == &cleanup_thread_);

  lockScheduling();
  uint32 thread_count_max =
      sizeof(cleanup_thread_.kernel_stack_) / (2 * sizeof(Thread *));
  thread_count_max = aostl::min(thread_count_max, threads_.size());
  Thread *destroy_list[thread_count_max];
  uint32 thread_count = 0;
  for (uint32 i = 0; i < threads_.size() && thread_count < thread_count_max;
       ++i) {
    Thread *tmp = threads_[i];
    if (tmp->getState() == ToBeDestroyed) {
      destroy_list[thread_count++] = tmp;
      threads_.erase(threads_.begin() + i); // Note: erase will not realloc!
      --i;
    }
  }
  unlockScheduling();
  if (thread_count > 0) {
    ArchThreads::atomic_set(unfinished_cleanup_counter_, thread_count);
    for (uint32 i = 0; i < thread_count; ++i) {
      delete destroy_list[i];
      ArchThreads::atomic_add(unfinished_cleanup_counter_, -1);
    }
    debug(SCHEDULER, "cleanupDeadThreads: done\n");
  }
}

void Scheduler::printThreadList() {
  lockScheduling();
  debug(SCHEDULER, "Scheduler::printThreadList: %zd Threads in List\n",
        threads_.size());
  for (size_t c = 0; c < threads_.size(); ++c)
    debug(SCHEDULER,
          "Scheduler::printThreadList: threads_[%zd]: %p  %zd:%25s     [%s]\n",
          c, threads_[c], threads_[c]->getTID(), threads_[c]->getName(),
          Thread::threadStatePrintable[threads_[c]->state_]);
  unlockScheduling();
}

void Scheduler::lockScheduling() // not as severe as stopping Interrupts
{
  if (unlikely(ArchThreads::testSetLock(block_scheduling_, 1)))
    kpanict("FATAL ERROR: Scheduler::*: block_scheduling_ was set !! How the "
            "Hell did the program flow get here then ?\n");
}

void Scheduler::unlockScheduling() { block_scheduling_ = 0; }

bool Scheduler::isSchedulingEnabled() { return this && block_scheduling_ == 0; }

bool Scheduler::isCurrentlyCleaningUp() {
  return currentThread == &cleanup_thread_;
}

size_t Scheduler::getTicks() { return ticks_; }

void Scheduler::incTicks() { ++ticks_; }

void Scheduler::printStackTraces() {
  lockScheduling();
  debug(BACKTRACE, "printing the backtraces of <%zd> threads:\n",
        threads_.size());

  for (const auto &thread : threads_) {
    thread->printBacktrace();
    debug(BACKTRACE, "\n");
    debug(BACKTRACE, "\n");
  }

  unlockScheduling();
}

void Scheduler::printLockingInformation() {
  lockScheduling();
  kprintfd("\n");
  debug(LOCK, "Scheduler::printLockingInformation:\n");

  for (Thread *t : threads_) {
    if (t->holding_lock_list_)
      Lock::printHoldingList(t);
  }
  for (Thread *t : threads_) {
    if (t->lock_waiting_on_)
      debug(LOCK, "Thread %s (%p) is waiting on lock: %s (%p).\n", t->getName(),
            t, t->lock_waiting_on_->getName(), t->lock_waiting_on_);
  }
  debug(LOCK, "Scheduler::printLockingInformation finished\n");
  unlockScheduling();
}

uint32 Scheduler::getThreadCount() { return threads_.size(); }

uint64 Scheduler::readTimeStampCounter() {
  // Source https://en.wikipedia.org/wiki/Time_Stamp_Counter
  // rdtsc -> This register counts the number of CPU cycles since its reset.
  // The instruction RDTSC returns the TSC in EDX:EAX

  uint64_t low, high;
  __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));

  uint64_t combined_registers = (high << 32) | low;
  return combined_registers;
}

void Scheduler::calculateCPUFrequency() {
  if (cycles_per_tick_calibrated_)
    return;

  uint64 now = readTimeStampCounter();

  if (last_calibration_tick_ == 0) {
    if (ticks_ <= CPU_CALIBRATION_WARMUP)
      return;

    last_calibration_tick_ = ticks_;
    first_timestamp_counter_ = now;
    return;
  }

  size_t tick_delta = ticks_ - last_calibration_tick_;
  if (tick_delta < CPU_CALIBRATION_TICKS)
    return;

  uint64 cycle_delta = now - first_timestamp_counter_;
  if (cycle_delta == 0)
    return;

  uint64 new_cycles_per_tick = cycle_delta / tick_delta;
  if (new_cycles_per_tick == 0)
    return;

  cycles_per_tick_ = new_cycles_per_tick;
  cpu_frequency_ = cycles_per_tick_ * 1000 / IRQ0_TIME;

  cycles_per_tick_calibrated_ = true;

  debug(SCHEDULER,
        "Scheduler::calculateCPUFrequency: CPU Frequency is %zu Hz | "
        "cycles_per_tick_ %ld\n",
        cpu_frequency_, cycles_per_tick_);
}

bool Scheduler::isCpuFrequencyReady() const {
  return cycles_per_tick_calibrated_;
}

char *Scheduler::getUpTimeString() {
  // XXh:XXm:XXs

  static char buffer[32]; // enough space

  size_t total_ms = ticks_ * 55;
  size_t seconds = (total_ms / 1000) % 60;
  size_t minutes = (total_ms / (1000 * 60)) % 60;
  size_t hours = (total_ms / (1000 * 60 * 60));

  char tmp[12];
  char *p = buffer;

  // hours
  itoa(hours, tmp, 10);
  for (char *t = tmp; *t; t++) {
    *p = *t;
    p++;
  }
  *p++ = 'h';
  *p++ = ':';

  // minutes
  itoa(minutes, tmp, 10);
  for (char *t = tmp; *t; t++) {
    *p = *t;
    p++;
  }
  *p++ = 'm';
  *p++ = ':';

  // seconds
  itoa(seconds, tmp, 10);
  for (char *t = tmp; *t; t++) {
    *p = *t;
    p++;
  }
  *p++ = 's';

  *p = '\0';

  // debug(SCHEDULER, "Scheduler::getUpTimeString: Uptime is %s\n", buffer);
  return buffer;
}

size_t Scheduler::getRandomNumber(size_t max_value) {
  uint64 tsc = readTimeStampCounter();
  size_t random_number = ((tsc * PRIME_NUMBER) % max_value);
  return random_number;
}

size_t Scheduler::getRandomNumberInRange(size_t min_value, size_t max_value) {
  assert(max_value > min_value && "max_value must be greater than min_value");

  size_t range = max_value - min_value;
  size_t random_number = getRandomNumber(range) + min_value;

  return random_number;
}
