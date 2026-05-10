#include "AppRegistry.h"
#include "FrameAlloc.h"
#include "Scheduler.h"
#include "UserProcess.h"
#include "FsOps.h"
#include "Vfs.h"
#include "kprintf.h"
#include "ScopeLock.h"
#include <memory/KernelHeap.h>

AppRegistry *AppRegistry::instance_ = 0;

AppRegistry::AppRegistry(FsContext *root_fs_info,
                                 char const *progs[])
    : Thread(root_fs_info, "AppRegistry", Thread::KERNEL_THREAD),
      progs_(progs), progs_running_(0),
      counter_lock_("AppRegistry::counter_lock_"),
      all_processes_killed_(&counter_lock_,
                            "AppRegistry::all_processes_killed_"),
      exit_status_lock_("AppRegistry::exit_status_lock_"),
      exit_status_cond_(&exit_status_lock_,
                        "AppRegistry::exit_status_cond_") {
  instance_ = this; // instance_ is static! -> Singleton-like behaviour
}

AppRegistry::~AppRegistry() {}

AppRegistry *AppRegistry::instance() { return instance_; }

void AppRegistry::Run() {
  if (!progs_ || !progs_[0])
    return;

  while (!Scheduler::instance()->isCpuFrequencyReady())
    Scheduler::instance()->yield();

  debug(PROCESS_REG, "mounting userprog-partition \n");

  debug(PROCESS_REG, "mkdir /usr\n");
  assert(!FsOps::mkdir("/usr", 0));
  debug(PROCESS_REG, "mount idea1\n");
  assert(!FsOps::mount("idea1", "/usr", "minixfs", 0));

  debug(PROCESS_REG, "mkdir /dev\n");
  assert(!FsOps::mkdir("/dev", 0));
  debug(PROCESS_REG, "mount devicefs\n");
  assert(!FsOps::mount(NULL, "/dev", "devicefs", 0));

  KernelHeap::instance()->startTracing();

  for (uint32 i = 0; progs_[i]; i++) {
    createProcess(progs_[i]);
  }

  counter_lock_.acquire();

  while (progs_running_)
    all_processes_killed_.wait();

  counter_lock_.release();

  debug(PROCESS_REG,
        "unmounting userprog-partition because all processes terminated \n");

  FsOps::umount("/usr", 0);
  FsOps::umount("/dev", 0);
  vfs.rootUmount();

  Scheduler::instance()->printStackTraces();
  Scheduler::instance()->printThreadList();

  FrameAlloc *pm = FrameAlloc::instance();
  if (!DYNAMIC_KMM && pm->getNumFreePages() != pm->getNumPagesForUser()) {
    FrameAlloc::instance()->printBitmap();
    debug(PM,
          "WARNING: You might be leaking physical memory pages somewhere\n");
    debug(PM, "%u/%u free physical pages after unmounting detected\n",
          pm->getNumFreePages(), pm->getNumPagesForUser());
  }

  kill();
}

void AppRegistry::processExit() {
  counter_lock_.acquire();

  if (--progs_running_ == 0)
    all_processes_killed_.signal();

  debug(PROCESS_REG, "processExit: now %u processes running\n", progs_running_);

  counter_lock_.release();
}

void AppRegistry::processStart() {
  counter_lock_.acquire();
  ++progs_running_;
  debug(PROCESS_REG, "processStart: now %u processes running\n",
        progs_running_);
  counter_lock_.release();
}

size_t AppRegistry::processCount() {
  ScopeLock lock(counter_lock_);
  return progs_running_;
}

void AppRegistry::createProcess(const char *path) {
  debug(PROCESS_REG, "create process %s\n", path);
  UserProcess *userprocess =
      new UserProcess(path, new FsContext(*working_dir_));

  if (!userprocess->init_success_) {
    debug(PROCESS_REG, "creating userprocess %s failed\n", path);
    delete userprocess;
  }

  debug(PROCESS_REG, "created userprocess %s\n", path);
}

void AppRegistry::registerProcess(UserProcess *process) {
  if (!process)
    return;

  ScopeLock lock(exit_status_lock_);

  live_processes_.push_back(process->getPID());
}

void AppRegistry::notifyProcessExit(UserProcess *process,
                                        int exit_status) {
  if (!process)
    return;

  ScopeLock lock(exit_status_lock_);

  const size_t pid = process->getPID();
  for (auto it = live_processes_.begin(); it != live_processes_.end(); ++it) {
    if (*it == pid) {
      live_processes_.erase(it);
      break;
    }
  }

  exit_status_map_[pid] = exit_status;
  exit_status_cond_.broadcast();
}

size_t AppRegistry::waitForProcess(size_t pid, int *status_out) {
  ScopeLock lock(exit_status_lock_);

  auto status_it = exit_status_map_.find(pid);
  bool known_process = status_it != exit_status_map_.end();
  if (!known_process) {
    for (size_t live_pid : live_processes_) {
      if (live_pid == pid) {
        known_process = true;
        break;
      }
    }
  }

  if (!known_process)
    return static_cast<size_t>(-1);

  while (exit_status_map_.find(pid) == exit_status_map_.end())
    exit_status_cond_.wait();

  int exited_status = exit_status_map_[pid];
  exit_status_map_.erase(pid);

  if (status_out)
    *status_out = exited_status;

  return pid;
}
