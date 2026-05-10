#include "UserProcess.h"
#include "ArchThreads.h"
#include "Console.h"
#include "File.h"
#include "HeapIface.h"
#include "Loader.h"
#include "FrameAlloc.h"
#include "AppRegistry.h"
#include "Scheduler.h"
#include "ShMemRegistry.h"
#include "StackAllocator.h"
#include "UserThread.h"
#include "FsOps.h"
#include "debug.h"
#include "kprintf.h"
#include "offsets.h"
#include "types.h"
#include "uvector.h"
#include <assert.h>

const char *UserProcess::processStatePrintable[5] = {
    "P_CREATING", "P_RUNNING", "P_FORK", "P_EXEC", "P_EXIT"};

size_t UserProcess::next_pid_ = 1;

UserProcess::UserProcess(aostl::string filename, FsContext *fs_info,
                         uint32 terminal_number)
    : threads_map_lock_("UserProcess::threads_map_lock_"),
      threads_map_lock_condition_(&threads_map_lock_,
                                  "UserProcess::threads_map_lock_condition_"),
      thread_lock_("UserProcess::thread_lock_"),
      thread_exit_mutex("UserProcess::thread_exit_mutex"),
      thread_exit_condition(&thread_exit_mutex,
                            "UserProcess::thread_exit_condition"),
      process_state_lock_("UserProcess::process_state_lock_"),
      process_state_condition_(&process_state_lock_,
                               "UserProcess::process_state_condition_"),
      thread_creation_mutex_("UserProcess::thread_creation_mutex_"),
      thread_creation_condition_(&thread_creation_mutex_,
                                 "UserProcess::thread_creation_condition_"),
      cleanup_exec_lock_("UserProcess::cleanup_exec_lock_")

{
  fs_info_ = fs_info;

  fd_ = FsOps::open(filename, O_RDONLY);
  init_success_ = false;
  next_tid_ = 0;
  last_scheduled_timestamp_ = 0;
  sum_scheduled_timestamp_ = 0;
  process_state_ = PROCESS_STATE::P_CREATING;
  thread_stack_manager_ = new StackAllocator(this);
  old_thread_stack_manager_ = nullptr;
  heap_manager_ = new HeapIface(this);
  old_heap_manager_ = nullptr;
  shared_mem_manager_ = nullptr;
  old_shared_mem_manager_ = nullptr;
  pid_ = ArchThreads::atomic_add(next_pid_, 1);
  parent_pid_ = 0;
  exit_status_ = 0;
  has_exited_ = false;
  loader_ = nullptr;
  thread_creation_counter_ = 0;
  old_loader_ = nullptr;
  old_fd_ = -1;
  cleanup_exec_ = false;

  if (fd_ >= 0)
    loader_ = new Loader(fd_);

  if (!loader_ || !loader_->loadExecutableAndInitProcess() ||
      !loader_->isValid()) {
    debug(USERPROCESS, "Error: loading %s failed!\n", filename.c_str());
    if (fd_ >= 0)
      FsOps::close(fd_);
    fd_ = -1;
    init_success_ = false;
    return;
  }

  shared_mem_manager_ = new ShMemRegistry(this, &loader_->arch_memory_);

  if (!fd_table_.registerDescriptor(fd_)) {
    debug(USERPROCESS, "Error: registering executable fd failed for %s\n",
          filename.c_str());
    FsOps::close(fd_);
    fd_ = -1;
    init_success_ = false;
    return;
  }

  minixfs_filename_ = filename;
  terminal_number_ = terminal_number;

  // first thread that is created for the process
  addThread(0, 0, 0, 0, 0, false);

  init_success_ = true;
  process_state_ = PROCESS_STATE::P_RUNNING;
  if (AppRegistry *registry = AppRegistry::instance()) {
    registry->registerProcess(this);
    registry->processStart();
  }

  debug(USERPROCESS, "ctor: Done loading %s\n", filename.c_str());
}

UserProcess::UserProcess(UserProcess *parent, UserThread *parent_thread)
    : threads_map_lock_("UserProcess::threads_map_lock_"),
      threads_map_lock_condition_(&threads_map_lock_,
                                  "UserProcess::threads_map_lock_condition_"),
      thread_lock_("UserProcess::thread_lock_"),
      thread_exit_mutex("UserProcess::thread_exit_mutex"),
      thread_exit_condition(&thread_exit_mutex,
                            "UserProcess::thread_exit_condition"),
      process_state_lock_("UserProcess::process_state_lock_"),
      process_state_condition_(&process_state_lock_,
                               "UserProcess::process_state_condition_"),
      thread_creation_mutex_("UserProcess::thread_creation_mutex_"),
      thread_creation_condition_(&thread_creation_mutex_,
                                 "UserProcess::thread_creation_condition_"),
      cleanup_exec_lock_("UserProcess::cleanup_exec_lock_") {

  assert(parent && parent_thread && "Fork constructor requires parent");

  old_fd_ = -1;
  old_loader_ = nullptr;
  old_thread_stack_manager_ = nullptr;
  old_heap_manager_ = nullptr;
  old_shared_mem_manager_ = nullptr;
  shared_mem_manager_ = nullptr;
  init_success_ = false;
  next_tid_ = parent->next_tid_;
  last_scheduled_timestamp_ = 0;
  sum_scheduled_timestamp_ = 0;
  process_state_ = PROCESS_STATE::P_CREATING;
  terminal_number_ = parent->terminal_number_;
  minixfs_filename_ = parent->minixfs_filename_;
  fs_info_ = new FsContext(*parent->fs_info_);
  thread_creation_counter_ = 0;
  cleanup_exec_ = false;
  old_fd_ = -1;
  fd_ = FsOps::open(minixfs_filename_.c_str(), O_RDONLY);
  if (fd_ < 0) {
    debug(USERPROCESS, "Error: reopening %s failed during fork!\n",
          minixfs_filename_.c_str());
    delete fs_info_;
    fs_info_ = nullptr;
    return;
  }
  pid_ = ArchThreads::atomic_add(next_pid_, 1);
  debug(USERPROCESS, "Child pid = %ld\n", pid_);

  parent_pid_ = parent->getPID();
  debug(USERPROCESS, "Parent pid = %ld\n", parent_pid_);
  exit_status_ = 0;
  has_exited_ = false;
  // Avoid holding parent arch_memory_lock_ here: Loader cloning can block on swap.
  loader_ = new Loader(*parent->loader_, fd_);
  if (!loader_ || !loader_->isValid()) {
    if (loader_) {
      delete loader_;
      loader_ = nullptr;
    }
    delete fs_info_;
    fs_info_ = nullptr;
    FsOps::close(fd_);
    fd_ = -1;
    return;
  }

  if (!fd_table_.registerDescriptor(fd_)) {
    debug(USERPROCESS, "Error: registering executable fd failed during fork\n");
    FsOps::close(fd_);
    fd_ = -1;
    return;
  }
  fd_table_.cloneFrom(parent->fileDescriptorTable());

  thread_stack_manager_ = new StackAllocator(parent->thread_stack_manager_);
  thread_stack_manager_->process_ = this;

  StackDescriptor *stack_info_copy = nullptr;
  if (parent->thread_stack_manager_)
    stack_info_copy = thread_stack_manager_->cloneStackInfoFrom(
        parent->thread_stack_manager_, parent_thread->getTID());

  UserThread *child_thread = new UserThread(this, parent_thread);
  if (!child_thread || !child_thread->init_success_) {
    if (child_thread)
      delete child_thread;
    delete loader_;
    loader_ = nullptr;
    delete thread_stack_manager_;
    thread_stack_manager_ = nullptr;
    if (stack_info_copy)
      delete stack_info_copy;
    delete fs_info_;
    fs_info_ = nullptr;
    fd_table_.closeAll();
    fd_ = -1;
    old_fd_ = -1;
    return;
  }

  if (stack_info_copy) {

    thread_stack_manager_->stack_info_lock_.acquire();
    thread_stack_manager_->setStackInfoForThread(child_thread->getTID(),
                                                 stack_info_copy);
    thread_stack_manager_->stack_info_lock_.release();
  }

  // copy heap manager state
  heap_manager_ = new HeapIface(parent->heap_manager_);
  heap_manager_->process_ = this;

  if (parent->shared_mem_manager_) {
    shared_mem_manager_ = new ShMemRegistry(
        *parent->shared_mem_manager_, this, &loader_->arch_memory_);
  } else {
    shared_mem_manager_ = new ShMemRegistry(this, &loader_->arch_memory_);
  }

  init_success_ = true;
  process_state_ = PROCESS_STATE::P_RUNNING;
  if (AppRegistry *registry = AppRegistry::instance()) {
    registry->registerProcess(this);
    registry->processStart();
  }
  addNewThreadToThreadsMap(child_thread);
  Scheduler::instance()->addNewThread(child_thread);
  debug(USERPROCESS, "fork ctor: cloned process from %s\n",
        minixfs_filename_.c_str());
}

UserProcess::~UserProcess() {
  debug(USERPROCESS, "Cleaning up process \n");

  assert(threads_.empty() &&
         "All threads should be removed from the process before destruction");

  if (fs_info_) {
    delete fs_info_;
    fs_info_ = nullptr;
  }

  if (shared_mem_manager_) {
    if (loader_)
      shared_mem_manager_->unmapAllPages(&loader_->arch_memory_);
    delete shared_mem_manager_;
    shared_mem_manager_ = nullptr;
  }

  if (old_shared_mem_manager_) {
    delete old_shared_mem_manager_;
    old_shared_mem_manager_ = nullptr;
  }

  if (old_loader_) {
    delete old_loader_;
    old_loader_ = nullptr;
  }

  if (loader_) {
    delete loader_;
    loader_ = nullptr;
  }

  fd_table_.closeAll();
  fd_ = -1;
  old_fd_ = -1;

  if (thread_stack_manager_) {
    delete thread_stack_manager_;
    thread_stack_manager_ = nullptr;
  }

  if (!init_success_)
    return;

  assert(Scheduler::instance()->isCurrentlyCleaningUp() &&
         "Process destruction must be done by the cleanup thread");

  if (AppRegistry *registry = AppRegistry::instance())
    registry->processExit();
}

void UserProcess::setExitStatus(int status) {
  exit_status_ = status;
  has_exited_ = true;
}

size_t UserProcess::addThread(size_t *thread, size_t pthread_attr_t,
                              void *(*start_routine)(void *), void *arg,
                              size_t wrapper, bool exec) {

  debug(USERPROCESS, "UserProcess::addThread() called\n");
  // TODO pthread_attr_t is currently ignored
  (void)pthread_attr_t;

  process_state_lock_.acquire();
  // For EXIT and EXEC
  if (isThreadCreationDisabled()) {
    process_state_lock_.release();
    debug(USERPROCESS, "Error: Thread creation is disabled for process %s\n",
          minixfs_filename_.c_str());
    return -1;
  }
  // abood implement wait mechanism for FORK
  while (isThreadCreationPaused()) {
    process_state_condition_.wait();
    if (isThreadCreationDisabled()) {
      process_state_lock_.release();
      debug(USERPROCESS, "Error: Thread creation is disabled for process %s\n",
            minixfs_filename_.c_str());
      return -1;
    }
  }
  // end abood

  increaseThreadCreationCounter();

  process_state_lock_.release();

  debug(USERPROCESS, "Adding new thread to process, filename: %s\n",
        minixfs_filename_.c_str());

  // thread_exit_mutex.acquire();
  UserThread *user_thread =
      new UserThread(this, start_routine, arg, wrapper, exec);

  if (!user_thread->init_success_ || user_thread == nullptr) {
    debug(USERPROCESS, "Error: creating thread failed!\n");
    decreaseThreadCreationCounter();
    return -1;
  }

  if (thread != nullptr)
    // write the thread id back to userspace, only if created with
    // pthread_create
    *thread = user_thread->getTID();

  addNewThreadToThreadsMap(user_thread);
  Scheduler::instance()->addNewThread(user_thread);

  decreaseThreadCreationCounter();
  debug(USERPROCESS, "UserProcess::addThread() done\n");

  return 0;
}

aostl::map<size_t, UserThread *> UserProcess::getThreadMap() {
  assert(threads_map_lock_.isHeldBy(currentThread) &&
         "getThreadMap() called without holding the lock");
  return threads_;
}

UserThread *UserProcess::getThreadById(size_t tid) {
  UserThread *thread = nullptr;

  threads_map_lock_.acquire();
  auto it = threads_.find(tid);
  if (it != threads_.end())
    thread = it->second;
  threads_map_lock_.release();

  return thread;
}

void UserProcess::onDescriptorClosed(int32 fd) {
  if (fd == fd_)
    fd_ = -1;
  if (fd == old_fd_)
    old_fd_ = -1;
}

void UserProcess::killMyself() {
  debug(USERPROCESS, "KillMyself() called | Killing process\n");
  delete this;
  debug(USERPROCESS, "Process killed\n");
}

size_t UserProcess::getNextTID() {
  return ArchThreads::atomic_add(next_tid_, 1);
}

void UserProcess::addNewThreadToThreadsMap(UserThread *thread) {
  threads_map_lock_.acquire();
  threads_.insert(aostl::make_pair(thread->getTID(), thread));
  threads_map_lock_condition_.signal();
  threads_map_lock_.release();
}

void UserProcess::removeThreadFromThreadsMap(UserThread *thread) {
  assert(currentThread == thread &&
         "removeThreadFromThreadsMap() called from a different thread");
  threads_map_lock_.acquire();
  debug(
      USERPROCESS,
      "Removing thread TID: %zu from process threads map | current size: %zu\n",
      thread->getTID(), threads_.size());
  threads_.erase(thread->getTID());

  // if there is only one thread left the process can be killed
  if (threads_.empty())
    thread->can_kill_process_ = true;

  threads_map_lock_condition_.signal();
  threads_map_lock_.release();
}

void UserProcess::clockInTimestamp() {
  last_scheduled_timestamp_ = Scheduler::instance()->readTimeStampCounter();
}

void UserProcess::clockOutTimestamp() {
  sum_scheduled_timestamp_ +=
      Scheduler::instance()->readTimeStampCounter() - last_scheduled_timestamp_;
}

// To determine the time in seconds, the value returned by clock() should be
// divided by the value of the macro CLOCKS_PER_SEC
uint64_t UserProcess::getClockCyclesRun() {
  const uint64_t cpu_frequency = Scheduler::instance()->cpu_frequency_;
  const uint64_t now = Scheduler::instance()->readTimeStampCounter();

  if (cpu_frequency == 0)
    return 0;

  uint64_t cycles_until_now =
      sum_scheduled_timestamp_ + (now - last_scheduled_timestamp_);

  uint64_t whole_seconds = cycles_until_now / cpu_frequency;
  uint64_t remaining_cycles = cycles_until_now % cpu_frequency;

  uint64_t clock_ticks = whole_seconds * CLOCKS_PER_SEC;
  uint64_t fractional =
      (remaining_cycles * CLOCKS_PER_SEC + cpu_frequency / 2) / cpu_frequency;
  clock_ticks += fractional;

  uint64_t bias = (clock_ticks >> 6) + (clock_ticks >> 9) + (clock_ticks >> 10);
  clock_ticks += bias;

  return clock_ticks;
}

const char *UserProcess::getStateString() {
  return processStatePrintable[process_state_];
}

void UserProcess::setProcessState(PROCESS_STATE process_state) {
  debug(USERPROCESS, "Setting process state to %s\n",
        processStatePrintable[process_state]);
  process_state_lock_.acquire();
  process_state_ = process_state;
  process_state_condition_.broadcast();
  process_state_lock_.release();
}

void UserProcess::increaseThreadCreationCounter() {
  thread_creation_mutex_.acquire();
  UserThread *user_thread = (UserThread *)currentThread;
  assert(!user_thread->is_creating_new_thread_ &&
         "Thread is already creating a new thread!");
  thread_creation_counter_++;
  user_thread->is_creating_new_thread_ = true;
  assert(thread_creation_counter_ >= 0 &&
         "Thread creation counter went below zero!");
  debug(USERPROCESS, "Increased thread creation counter to %d\n",
        thread_creation_counter_);
  thread_creation_condition_.signal();
  thread_creation_mutex_.release();
}

void UserProcess::decreaseThreadCreationCounter() {
  thread_creation_mutex_.acquire();
  UserThread *user_thread = (UserThread *)currentThread;
  assert(user_thread->is_creating_new_thread_ &&
         "Thread is not creating a new thread!");
  thread_creation_counter_--;
  user_thread->is_creating_new_thread_ = false;
  assert(thread_creation_counter_ >= 0 &&
         "Thread creation counter went below zero!");
  debug(USERPROCESS, "Decreased thread creation counter to %d\n",
        thread_creation_counter_);
  thread_creation_condition_.signal();
  thread_creation_mutex_.release();
}

void UserProcess::waitUntilAllThreadsAreCreated() {
  thread_creation_mutex_.acquire();
  while (thread_creation_counter_ > 0) {
    debug(USERPROCESS,
          "Waiting until all threads are created, current counter: %d\n",
          thread_creation_counter_);
    thread_creation_condition_.wait();
  }
  thread_creation_mutex_.release();
}

bool UserProcess::isThreadCreationDisabled() {
  assert(process_state_lock_.isHeldBy(currentThread) &&
         "isThreadCreationDisabled() called without holding the lock");
  return process_state_ == PROCESS_STATE::P_EXEC ||
         process_state_ == PROCESS_STATE::P_EXIT;
}

bool UserProcess::isThreadCreationPaused() {
  assert(process_state_lock_.isHeldBy(currentThread) &&
         "isThreadCreationPaused() called without holding the lock");
  return process_state_ == PROCESS_STATE::P_FORK;
}

void UserProcess::waitUntilAllThreadsAreDeadExceptCalling() {
  threads_map_lock_.acquire();
  while (threads_.size() > 1) {
    debug(USERPROCESS,
          "Waiting until all threads are dead except calling, current "
          "threads: %zu\n",
          threads_.size());
    threads_map_lock_condition_.wait();
  }
  assert(threads_.size() == 1 &&
         "There should be only one thread left (the calling thread)");
  threads_map_lock_.release();
}

void UserProcess::markAllThreadsToKilledExceptCalling() {
  UserThread *current_thread = (UserThread *)currentThread;
  void *const pthread_canceled = (void *)PTHREAD_CANCELED;

  thread_lock_.acquire();
  thread_exit_mutex.acquire();

  aostl::vector<size_t> tids_to_cancel;
  tids_to_cancel.reserve(threads_.size());

  threads_map_lock_.acquire();
  for (auto it = threads_.begin(); it != threads_.end(); ++it) {
    if (it->second != current_thread)
      tids_to_cancel.push_back(it->first);
  }
  threads_map_lock_.release();

  for (size_t tid : tids_to_cancel) {
    UserThread *thread = getThreadById(tid);
    if (!thread || thread == current_thread)
      continue;

    thread->setCancelState(0); // enable
    thread->setCancelType(1);  // asynchronous
    thread->setThreadAsCanceled();

    if (!thread->isDetached()) {
      threads_ret_val[thread->getTID()] = pthread_canceled;
    } else {
      threads_ret_val.erase(thread->getTID());
    }
  }
  fd_table_.shutdownPipes();

  thread_exit_condition.broadcast();
  thread_exit_mutex.release();
  thread_lock_.release();
}

size_t UserProcess::handleExecProcess(const char *path, char *const argv[]) {

  // Check argv validity
  // Doing argv checks before everything else
  StackAllocator *new_thread_stack_manager_ = new StackAllocator(this);
  if (!new_thread_stack_manager_->writeExecParamPage(argv)) {
    delete new_thread_stack_manager_;
    process_state_lock_.acquire();
    process_state_ = PROCESS_STATE::P_RUNNING;
    process_state_condition_.broadcast();
    process_state_lock_.release();
    debug(USERPROCESS, "Exec: Exiting handleExecProcess with error\n");
    return -1;
  }

  debug(USERPROCESS, "Exec: Argv checks done\n");

  debug(USERPROCESS, "Exec: Trying to open new image %s\n", path);
  // Check if path exists and is accessible
  int32 new_fd = FsOps::open(path, O_RDONLY);
  Loader *new_loader = nullptr;

  if (new_fd >= 0)
    new_loader = new Loader(new_fd);

  debug(USERPROCESS, "Exec: Loader created for new image\n");

  if (!new_loader || !new_loader->loadExecutableAndInitProcess() ||
      !new_loader->isValid()) {
    debug(USERPROCESS, "Error: Exec loading %s failed!\n", path);
    delete new_thread_stack_manager_;
    process_state_lock_.acquire();
    process_state_ = PROCESS_STATE::P_RUNNING;
    process_state_condition_.broadcast();
    process_state_lock_.release();
    return -1;
  }

  debug(USERPROCESS, "Exec: Init of new loader done\n");

  if (!fd_table_.registerDescriptor(new_fd)) {
    debug(USERPROCESS,
          "Exec: registering executable fd failed for new image %s\n", path);
    delete new_thread_stack_manager_;
    delete new_loader;
    FsOps::close(new_fd);
    process_state_lock_.acquire();
    process_state_ = PROCESS_STATE::P_RUNNING;
    process_state_condition_.broadcast();
    process_state_lock_.release();
    return -1;
  }

  debug(USERPROCESS, "Exec: New fd registered\n");

  // create the new heap manager for the new process image
  HeapIface *new_heap_manager = new HeapIface(this);
  debug(USERPROCESS, "Exec: New heap manager created\n");

  ShMemRegistry *new_shared_mem_manager =
      new ShMemRegistry(this, &new_loader->arch_memory_);

  //
  // If a thread reaches this point, exec is guaranteed to succeed -> must
  // succeed lol
  //

  // Save old loader and fd to be able to cleanup after exec
  cleanup_exec_lock_.acquire();
  old_loader_ = loader_;
  old_fd_ = fd_;
  aostl::string old_filename = minixfs_filename_;
  old_thread_stack_manager_ = thread_stack_manager_;
  old_heap_manager_ = heap_manager_;
  old_shared_mem_manager_ = shared_mem_manager_;

  // Switch to new loader and fd
  loader_ = new_loader;
  fd_ = new_fd;
  minixfs_filename_ = aostl::string(path);
  thread_stack_manager_ = new_thread_stack_manager_;
  heap_manager_ = new_heap_manager;
  shared_mem_manager_ = new_shared_mem_manager;

  // unmap old managers

  cleanup_exec_lock_.release();

  waitUntilAllThreadsAreCreated();

  fd_table_.shutdownPipes();

  markAllThreadsToKilledExceptCalling();

  // Set process state back to P_RUNNING so we can create threads again
  process_state_lock_.acquire();
  process_state_ = PROCESS_STATE::P_RUNNING;
  // also wake up any threads that wanted to also call EXEC
  process_state_condition_.broadcast();
  process_state_lock_.release();

  waitUntilAllThreadsAreDeadExceptCalling();

  // Thread is not allowed to delete its own loader during kill
  ((UserThread *)currentThread)->is_exec_calling_thread_ = true;

  // kill the calling thread
  currentThread->kill();

  assert(false && "Should not reach this point in EXEC\n");
  return 9999;
}

void UserProcess::cleanupAfterExec() {
  debug(USERPROCESS, "Cleaning up after called!\n");

  cleanup_exec_lock_.acquire();
  if (cleanup_exec_) {
    if (old_shared_mem_manager_)
      delete old_shared_mem_manager_;
    if (old_loader_)
      delete old_loader_;
    if (old_thread_stack_manager_)
      delete old_thread_stack_manager_;
    if (old_heap_manager_)
      delete old_heap_manager_;
    old_loader_ = nullptr;
    old_thread_stack_manager_ = nullptr;
    old_heap_manager_ = nullptr;
    old_shared_mem_manager_ = nullptr;
    if (old_fd_ >= 0 && fd_table_.closeDescriptor(old_fd_))
      onDescriptorClosed(old_fd_);
    old_fd_ = -1;
    cleanup_exec_ = false;
    debug(USERPROCESS, "Cleanup after exec done!\n");
    cleanup_exec_lock_.release();
    return;
  }
  cleanup_exec_lock_.release();
  debug(USERPROCESS, "No cleanup after exec needed!\n");
}
