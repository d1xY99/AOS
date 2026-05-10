
#pragma once

#include "Mutex.h"
#include "Thread.h"
#include "StackAllocator.h"

class UserProcess;

class UserThread : public Thread {
public:
  UserThread(UserProcess *process, void *(*start_routine)(void *), void *arg,
             size_t wrapper, bool exec);

  UserThread(UserProcess *process, UserThread *parent_thread);

  virtual ~UserThread();

  virtual void Run(); // not used

  bool schedulable() override;
  void kill() override;
  size_t sendThreadToSleep(size_t seconds);

  // This is the process  this thread belongs to
  UserProcess *process_;

  bool init_success_;
  bool can_kill_process_;
  bool is_exec_calling_thread_;

  // if this thread is currently creating a new thread
  bool is_creating_new_thread_;

  void setCancelState(int value);
  void setCancelType(int value);
  int getCancelState();
  int getCancelType();
  void setThreadAsCanceled() { is_canceled_.store(true); }
  bool isCanceled() const;
  bool isThisThreadCanceled() const { return isCanceled(); }
  void setDetached() { is_detached_.store(true); }
  bool isDetached() const { return is_detached_.load(); }

  Mutex cancel_statType_mutex_;

  void printThreadInformation();

  void setStackInfo(pthread_thread_info *info);

private:
  uint64_t wake_up_time_stamp_;

  void setWakeUpTimeStamp(size_t seconds);
  bool isThreadStillWaiting();
  pthread_thread_info *stack_info_;

  aostl::atomic<bool> is_canceled_;
  aostl::atomic<bool> cancel_type_; // DEFERRED (default == 0), ASYNCHRONOUS == 1
  aostl::atomic<bool> cancel_state_; // CANCEL_ENABLE == 0, CANCEL_DISABLE == 1.
  aostl::atomic<bool> is_dead_;
  aostl::atomic<bool> is_detached_;
};
