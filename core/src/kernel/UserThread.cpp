
#include "UserThread.h"
#include "ArchInterrupts.h"
#include "ArchThreads.h"
#include "Console.h"
#include "File.h"
#include "Loader.h"
#include "FrameAlloc.h"
#include "AppRegistry.h"
#include "Scheduler.h"
#include "Thread.h"
#include "UserProcess.h"
#include "FsOps.h"
#include "debug.h"
#include "kprintf.h"
#include "offsets.h"
#include "paging-definitions.h"
// #include "Mutex.h"
#include "Syscall.h"
#include "kernel_user_shared.h"

UserThread::UserThread(UserProcess *process, void *(*start_routine)(void *),
                       void *arg, size_t wrapper, bool exec)
    : Thread(process->fs_info_, process->minixfs_filename_, Thread::USER_THREAD,
             process->getNextTID()),
      cancel_statType_mutex_("UserThread::cancel_statType_mutex_") {
  is_canceled_.store(false);
  is_dead_.store(false);
  cancel_state_.store(false); // enabled
  cancel_type_.store(0);      // deferred
  is_detached_.store(false);
  stack_info_ = nullptr;
  is_exec_calling_thread_ = false;
  can_kill_process_ = false;

  process_ = process;
  size_t tid = getTID();
  debug(USERTHREAD, "UserThread ctor: Creating new user thread with TID: %zu\n",
        tid);
  loader_ = process->loader_;
  wake_up_time_stamp_ = 0;
  init_success_ = false;
  is_creating_new_thread_ = false;

  void *stack_start = process_->thread_stack_manager_->setupThreadStack(this);

  void *loader_start_routine = (void *)loader_->getEntryFunction();
  if (tid == 0) {
    debug(USERTHREAD, "UserThread ctor: Setting up first thread\n");
    ArchThreads::createUserRegisters(
        user_registers_, loader_start_routine,
        (void *)((size_t)stack_start - sizeof(pointer)),
        getKernelStackStartPointer());

    if (exec) {
      debug(USERTHREAD, "UserThread ctor: This is thread created for EXEC\n");
      user_registers_->rdi =
          (uint64_t)process_->thread_stack_manager_->getArgvCount();
      user_registers_->rsi =
          (uint64_t)process_->thread_stack_manager_->readExecParamPage();
    }
  } else if (tid > 0) {
    (void)start_routine;
    (void)arg;
    debug(USERTHREAD, "UserThread ctor: Setting up new thread\n");
    ArchThreads::createUserRegisters(
        user_registers_, (void *)wrapper,
        (void *)((size_t)stack_start - sizeof(pointer)),
        getKernelStackStartPointer());
    user_registers_->rdi = (uint64_t)start_routine;
    user_registers_->rsi = (uint64_t)arg;
  }

  // if tls set the fs base
  if (process_->loader_->tls_info_.found) {

    process->thread_stack_manager_->stack_info_lock_.acquire();
    StackDescriptor *stack_info =
        process->thread_stack_manager_->getStackInfoForThread(tid);
    process->thread_stack_manager_->stack_info_lock_.release();

    if (stack_info && stack_info->pthread_info_start) {
      debug(USERTHREAD,
            "UserThread ctor: Setting FS base to TLS address %18zx for thread "
            "%zu\n",
            (size_t)stack_info->pthread_info_start, tid);
      user_registers_->fs = (size_t)stack_info->pthread_info_start;
    } else {
      debug(USERTHREAD,
            "UserThread ctor: TLS metadata missing for thread %zu, leaving FS "
            "base unchanged\n",
            tid);
    }
  }

  ArchThreads::setAddressSpace(this, loader_->arch_memory_);

  if (main_console->getTerminal(process->terminal_number_))
    setTerminal(main_console->getTerminal(process->terminal_number_));

  init_success_ = true;
  switch_to_userspace_ = 1;
}

UserThread::UserThread(UserProcess *process, UserThread *parent_thread)
    : Thread(process->fs_info_, process->minixfs_filename_, Thread::USER_THREAD,
             parent_thread ? parent_thread->getTID() : 0),
      cancel_statType_mutex_("UserThread::cancel_statType_mutex_") {
  assert(parent_thread && "Forked thread requires a parent thread");

  process_ = process;
  loader_ = process_->loader_;
  wake_up_time_stamp_ = 0;
  init_success_ = false;
  is_exec_calling_thread_ = false;
  can_kill_process_ = false;

  is_canceled_.store(false);
  is_dead_.store(false);
  cancel_state_.store(parent_thread->cancel_state_.load());
  cancel_type_.store(parent_thread->cancel_type_.load());
  is_detached_.store(parent_thread->is_detached_.load());
  stack_info_ = nullptr;

  // Copy registers
  user_registers_ = new ArchThreadRegisters();
  memcpy(user_registers_, parent_thread->user_registers_,
         sizeof(ArchThreadRegisters));

  void *kernel_stack_top = getKernelStackStartPointer();

  // update stack pointers in user registers
  if (user_registers_)
    user_registers_->rsp0 = (size_t)kernel_stack_top;

  // In the child process, the return value of fork() is 0
  if (user_registers_)
    user_registers_->rax = 0;

  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  setTerminal(parent_thread->getTerminal());

  init_success_ = true;
  switch_to_userspace_ = 1;
}

UserThread::~UserThread() {

  if (is_exec_calling_thread_) {
    debug(USERTHREAD,
          "UserThread dtor: Calling thread of EXEC - marking process for "
          "cleanup after exec\n");
    process_->cleanup_exec_lock_.acquire();
    process_->cleanup_exec_ = true;
    process_->cleanup_exec_lock_.release();
    process_->cleanupAfterExec();
    process_->threads_.clear();

    process_->thread_stack_manager_->mapParamPages(process_->loader_);
    process_->next_tid_ = 0;
    debug(USERPROCESS, "Exec: Trying to add new thread\n");
    process_->addThread(0, 0, 0, 0, 0, true);

    debug(USERTHREAD,
          "UserThread dtor: Process cleanup after EXEC calling thread done\n");
    return;
  }

  if (can_kill_process_) {
    debug(USERTHREAD, "Last thread of process - cleaning up process\n");
    process_->killMyself();
    // :(
    debug(USERTHREAD,
          "UserThread dtor: Process cleanup after last thread done\n");
  }
}

void UserThread::Run() {
  debug(USERTHREAD, "Run: Fail-safe kernel panic - you probably have "
                    "forgotten to set switch_to_userspace_ = 1\n");
  assert(false);
}

void UserThread::setWakeUpTimeStamp(uint64_t miliseconds) {
  const uint64_t ms_per_tick = IRQ0_TIME;
  const uint64_t cycles_per_tick = Scheduler::instance()->cycles_per_tick_;
  const uint64_t now = Scheduler::instance()->readTimeStampCounter();

  uint64_t ms_total = miliseconds;

  uint64_t ticks = (uint64_t)(ms_total / ms_per_tick);
  uint64_t rem_ms = (uint64_t)(ms_total % ms_per_tick);

  // extra cycles for the partial tick (floor):
  uint64_t extra_cycles = (uint64_t)((rem_ms * cycles_per_tick) / ms_per_tick);

  uint64_t wake = ticks * cycles_per_tick + extra_cycles + now;

  wake_up_time_stamp_ = wake;

  debug(
      SYSCALL,
      "UserThread::setWakeUpTimeStamp: now %zu, miliseconds %zu, wake_up_time_"
      "stamp_ %zu\n",
      now, miliseconds, wake_up_time_stamp_);
}

size_t UserThread::sendThreadToSleep(size_t miliseconds) {
  debug(SYSCALL, "UserThread::sendThreadToSleep: %zd miliseconds\n",
        miliseconds);
  setWakeUpTimeStamp(miliseconds);
  setState(ThreadState::Waiting);
  ((UserThread *)currentThread)->process_->clockOutTimestamp();

  // Yield here because thread is not runnable anymore
  Scheduler::instance()->yield();

  // TODO: Needs to return the time remaining if the sleep was interrupted
  return 0;
}

bool UserThread::isThreadStillWaiting() {
  if (getState() != ThreadState::Waiting)
    return false;

  if (Scheduler::instance()->readTimeStampCounter() >= wake_up_time_stamp_)
    return false;

  return true;
}

bool UserThread::schedulable() {

  if (getCancelType() == 1 && isThisThreadCanceled() && getCancelState() == 0 &&
      switch_to_userspace_) {
    debug(SCHEDULER,
          "Scheduler::schedule: Thread %s should be asynchrounously canceled\n",
          currentThread->getName());
    kernel_registers_->rip = (uint64)&Syscall::pthread_exit;
    switch_to_userspace_ = 0;
    kernel_registers_->rdi = -1;
    return Thread::schedulable();
  }

  if (!isThreadStillWaiting() && getState() == ThreadState::Waiting) {
    debug(SYSCALL, "UserThread::schedulable: Woke up from sleep | %ld\n",
          Scheduler::instance()->readTimeStampCounter());
    setState(ThreadState::Running);
    wake_up_time_stamp_ = 0;
  }

  // Comment out because of swapping issues with userspace mutexes
  // Userspace mutex stuff
  // if (!this->stack_info_)
  //   return Thread::schedulable();
  //
  // assert(this->stack_info_ &&
  //        "UserThread::schedulable: stack_info_ is null pointer");
  // assert(this->stack_info_->thread_id == getTID() &&
  //        "UserThread::schedulable: stack_info_ thread_id mismatch");
  //
  // if (this->stack_info_->block_schedule &&
  //     this->stack_info_->waiting_list_blocked)
  //   return false;
  // End of userspace mutex stuff

  return Thread::schedulable();
}

void UserThread::kill() {
  assert(this == currentThread &&
         "UserThread::kill: Thread trying to kill another thread!");
  debug(USERTHREAD, "UserThread Kill called: Killing user thread TID: %zu\n",
        getTID());
  stack_info_ = nullptr;
  if (is_exec_calling_thread_) {
    debug(USERTHREAD,
          "UserThread::kill: Killing calling thread of EXEC TID: %zu\n",
          getTID());
    Thread::kill();
    return;
  }

  process_->thread_stack_manager_->unmapStackForThread(this);
  process_->removeThreadFromThreadsMap(this);

  if (!can_kill_process_) {
    setState(ToBeDestroyed);
    process_ = nullptr;
  }

  Thread::kill();
}

int UserThread::getCancelState() { return cancel_state_.load(); }

int UserThread::getCancelType() { return cancel_type_.load(); }

bool UserThread::isCanceled() const { return is_canceled_.load(); }

void UserThread::setCancelState(int state) { cancel_state_.store(state); }
void UserThread::setCancelType(int type) { cancel_type_.store(type); }

void UserThread::printThreadInformation() {
  debug(USERTHREAD,
        "Cancel State %d | Cancel Type %d | Is Canceled %d "
        "| Detached %d\n",
        getCancelState(), getCancelType(), isCanceled(), isDetached());
}

void UserThread::setStackInfo(pthread_thread_info *info) { stack_info_ = info; }
