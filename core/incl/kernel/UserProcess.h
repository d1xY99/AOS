#pragma once

#include "Condition.h"
#include "FsContext.h"
#include "HeapIface.h"
#include "FileSlotTable.h"
#include "Mutex.h"
#include "Thread.h"
#include "StackAllocator.h"
#include "UserThread.h"
#include "umap.h"

#define CLOCKS_PER_SEC 1000000
#define MAX_THREADS_PER_PROCESS 400

enum PROCESS_STATE { P_CREATING, P_RUNNING, P_FORK, P_EXEC, P_EXIT };

class ShMemRegistry;

class UserProcess {

  friend class UserThread;
  friend class StackAllocator;
  friend class HeapIface;

public:
  static const char *processStatePrintable[5];
  /**
   * Constructor
   * @param minixfs_filename filename of the file in minixfs to execute
   * @param fs_info filesysteminfo-object to be used
   * @param terminal_number the terminal to run in (default 0)
   *
   */
  UserProcess(aostl::string minixfs_filename, FsContext *fs_info,
              uint32 terminal_number = 0);

  UserProcess(UserProcess *parent, UserThread *parent_thread);

  virtual ~UserProcess();

  bool init_success_;
  bool cleanup_exec_;

  size_t addThread(size_t *thread, size_t pthread_attr_t,
                   void *(*start_routine)(void *), void *arg, size_t wrapper,
                   bool exec);

  aostl::map<size_t, UserThread *> getThreadMap();
  UserThread *getThreadById(size_t tid);
  aostl::map<size_t, void *> threads_ret_val;

  Mutex threads_map_lock_;
  Condition threads_map_lock_condition_;
  Mutex thread_lock_;
  Mutex thread_exit_mutex;
  Condition thread_exit_condition;
  aostl::map<size_t, size_t> thread_join_dependencies;

  void clockInTimestamp();
  void clockOutTimestamp();
  uint64_t getClockCyclesRun();

  StackAllocator *thread_stack_manager_;
  HeapIface *heap_manager_;
  ShMemRegistry *shared_mem_manager_;

  Mutex process_state_lock_;
  Condition process_state_condition_;
  PROCESS_STATE process_state_;
  void setProcessState(PROCESS_STATE process_state);
  const char *getStateString();

  size_t getPID() const { return pid_; }
  size_t getParentPID() const { return parent_pid_; }
  void setExitStatus(int status);
  int getExitStatus() const { return exit_status_; }
  bool hasExited() const { return has_exited_; }

  FileSlotTable &fileDescriptorTable() { return fd_table_; }
  const FileSlotTable &fileDescriptorTable() const {
    return fd_table_;
  }
  ShMemRegistry *sharedMemManager() const { return shared_mem_manager_; }

  void onDescriptorClosed(int32 fd);

  // for thread creation
  Mutex thread_creation_mutex_;
  Condition thread_creation_condition_;
  int thread_creation_counter_;
  void increaseThreadCreationCounter();
  void decreaseThreadCreationCounter();
  void waitUntilAllThreadsAreCreated();
  void waitUntilAllThreadsAreDeadExceptCalling();
  void markAllThreadsToKilledExceptCalling();

  // EXIT and EXEC disable thread creation
  // EXIT will not enable it again because the process will be killed
  // EXEC will enable it again after the exec is done
  bool isThreadCreationDisabled();

  // FORK pauses thread creation
  // after FORK is done thread creation is unpaused
  bool isThreadCreationPaused();

  size_t handleExecProcess(const char *path, char *const argv[]);

  void cleanupAfterExec();
  Mutex cleanup_exec_lock_;
  Loader *loader_;

  aostl::string minixfs_filename_;

  aostl::map<size_t, UserThread *> threads_;

private:
  int32 fd_;
  int32 old_fd_;
  Loader *old_loader_;
  StackAllocator *old_thread_stack_manager_;
  HeapIface *old_heap_manager_;
  ShMemRegistry *old_shared_mem_manager_;
  uint32 terminal_number_;
  FsContext *fs_info_;
  size_t next_tid_;
  size_t pid_;
  size_t parent_pid_;
  static size_t next_pid_;
  int exit_status_;
  bool has_exited_;

  // relevant for time()
  uint64 last_scheduled_timestamp_;
  uint64 sum_scheduled_timestamp_;

  FileSlotTable fd_table_;

  void killMyself();
  size_t getNextTID();
  void addNewThreadToThreadsMap(UserThread *thread);
  void removeThreadFromThreadsMap(UserThread *thread);
};
