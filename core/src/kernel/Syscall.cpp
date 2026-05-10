#include "Syscall.h"
#include "File.h"
#include "KernelHeap.h"
#include "Loader.h"
#include "FileSlot.h"
#include "FileSlotTable.h"
#include "FrameAlloc.h"
#include "Pipe.h"
#include "AppRegistry.h"
#include "Scheduler.h"
#include "ScopeLock.h"
#include "ShMemRegistry.h"
#include "SwapEngine.h"
#include "Terminal.h"
#include "UserProcess.h"
#include "UserThread.h"
#include "FsOps.h"
#include "Vfs.h"
#include "debug.h"
#include "debug_bochs.h"
#include "offsets.h"
#include "paging-definitions.h"
#include "kernel_user_shared.h"
#include "syscall-definitions.h"
#include <Inode.h>
#include <PathWalker.h>

// OS is insanity :(

namespace {
constexpr size_t ESRCH = 3;
constexpr size_t EINVAL = 22;
constexpr size_t EFAULT = 14;
constexpr size_t EDEADLK = 35;

bool is_user_thread() {
  return currentThread && currentThread->type_ == Thread::USER_THREAD;
}

bool fd_visible_to_current_process(size_t fd) {
  if (!is_user_thread()) {
    return true;
  }

  if (fd <= fd_stderr) {
    return true;
  }

  UserThread *user_thread = static_cast<UserThread *>(currentThread);
  if (!user_thread->process_) {
    return false;
  }

  return user_thread->process_->fileDescriptorTable().getKernelDescriptor(
             static_cast<int32>(fd)) != nullptr;
}
} // namespace

size_t Syscall::syscallException(size_t syscall_number, size_t arg1,
                                 size_t arg2, size_t arg3, size_t arg4,
                                 size_t arg5) {

  size_t return_value = 0;
  if ((syscall_number != sc_sched_yield) &&
      (syscall_number !=
       sc_outline)) // no debug  print because these might occur very often
  {
    debug(SYSCALL,
          "Syscall %zd called with arguments %zd(=%zx) %zd(=%zx) %zd(=%zx) "
          "%zd(=%zx) %zd(=%zx) | TID:%zu\n",
          syscall_number, arg1, arg1, arg2, arg2, arg3, arg3, arg4, arg4, arg5,
          arg5, currentThread->getTID());

    if (currentThread->type_ == Thread::USER_THREAD) {
      UserThread *user_thread = (UserThread *)currentThread;
      UserProcess *process = user_thread->process_;

      assert(process &&
             "Syscall::syscallException: UserThread has null process pointer");
      debug(SYSCALL, "Current Thread Process State %s | PID %ld | Name: %s\n",
            process->getStateString(), process->getPID(),
            process->minixfs_filename_.c_str());
      user_thread->printThreadInformation();
    }
  }

  handle_pending_cancel("entry");
  switch (syscall_number) {
  // Heap Stuff
  case sc_brk:
    return_value = brk((void *)arg1);
    break;
  case sc_sbrk:
    return_value = sbrk(arg1);
    break;
  //
  case sc_sched_yield:
    Scheduler::instance()->yield();
    break;
  case sc_createprocess:
    return_value = createprocess(arg1, arg2);
    break;
  case sc_fork:
    return_value = fork();
    break;
  case sc_execv:
    return_value = execv((const char *)arg1, (char *const *)arg2);
    break;
  case sc_waitpid:
    return_value = waitpid(arg1, (int *)arg2, arg3);
    break;
  case sc_exit:
    exit(arg1);
    break;
  case sc_write:
    return_value = write(arg1, arg2, arg3);
    break;
  case sc_read:
    return_value = read(arg1, arg2, arg3);
    break;
  case sc_lseek:
    return_value = lseek(arg1, static_cast<l_off_t>(arg2), arg3);
    break;
  case sc_open:
    return_value = open(arg1, arg2);
    break;
  case sc_close:
    return_value = close(arg1);
    break;
  case sc_outline:
    outline(arg1, arg2);
    break;
  case sc_trace:
    trace();
    break;
  case sc_pseudols:
    pseudols((const char *)arg1, (char *)arg2, arg3);
    break;
  case sc_threadcount:
    return_value = get_thread_count();
    break;
  case sc_boot_crash_test:
    boot_crash_test();
    break;
  case sc_sleep:
    return_value = sleep(arg1);
    break;
  case sc_usleep:
    return_value = usleep(arg1);
    break;
  case sc_clock:
    return_value = clock();
    break;
  case sc_mmap:
    return_value = (size_t)mmap(nullptr, arg1, (int)arg2, (int)arg3,
                                (int)arg4, (off_t)arg5);
    break;
  case sc_munmap:
    return_value = munmap((void *)arg1, arg2);
    break;
  case sc_shm_open:
    return_value = shm_open((const char *)arg1, (int)arg2, (mode_t)arg3);
    break;
  case sc_shm_unlink:
    return_value = shm_unlink((const char *)arg1);
    break;
  case sc_mprotect:
    return_value = mprotect((void *)arg1, arg2, (int)arg3);
    break;
    // START pthread functions
  case sc_pthread_create:
    return_value = pthread_create((size_t *)arg1, arg2, (void *(*)(void *))arg3,
                                  (void *)arg4, arg5);
    break;
  case sc_pthread_exit:
    pthread_exit((void *)arg1);
    break;
  case sc_pthread_cancel:
    return_value = pthread_cancel(arg1);
    break;
  case sc_pthread_detach:
    return_value = pthread_detach(arg1);
    break;
  case sc_pthread_setcancelstate:
    return_value = pthread_setcancelstate(static_cast<int>(arg1), (int *)arg2);
    break;
  case sc_pthread_setcanceltype:
    return_value = pthread_setcanceltype(static_cast<int>(arg1), (int *)arg2);
    break;
  case sc_pthread_join:
    return_value = pthread_join(arg1, (void **)arg2);
    break;
  case sc_pthread_self:
    return_value = pthread_self();
    break;
    // END pthread
  case sc_pipe:
    return_value = pipe((int *)arg1);
    break;
  case sc_dup:
    return_value = dup((int)arg1);
    break;
  case sc_dup2:
    return_value = dup2(static_cast<int>(arg1), static_cast<int>(arg2));
    break;
  case sc_rename:
    return_value = rename(arg1, arg2);
    break;
  case sc_ftruncate:
    return_value = ftruncate(arg1, arg2);
    break;
  case sc_truncate:
    return_value = truncate(arg1, arg2);
    break;
  case sc_access:
    return_value = access(arg1, arg2);
    break;
  case set_replacement_algo:
    return_value = set_replacement_algo_func(arg1);
    break;
  default:
    return_value = -1;
    kprintf("Syscall::syscallException: Unimplemented Syscall Number %zd\n",
            syscall_number);
  }
  handle_pending_cancel("exit");
  return return_value;
}

int Syscall::access(size_t path_ptr, int mode) {
  debug(ACCESS, "Syscall::access called\n");

  if (path_ptr < PAGE_SIZE || path_ptr >= USER_BREAK)
    return -1;

  const char *file_path = (const char *)path_ptr;

  if (file_path == nullptr)
    return -1;

  FsContext *fs_info = getcwd();

  Path target_path;
  Path parent_dir_path;
  int32 path_walk_status =
      PathWalker::pathWalk(file_path, fs_info, target_path, &parent_dir_path);
  uint32 file_mode;

  if (path_walk_status == PW_SUCCESS) {
    debug(ACCESS, "Found target file: %s\n", target_path.dentry_->getName());
    Inode *target_inode = target_path.dentry_->getInode();

    if (target_inode->getType() != I_FILE) {
      debug(ACCESS, "Error: This path is not a file\n");
      return -1;
    }
    file_mode = target_inode->getMode();
  } else
    return -1;

  if (mode == F_OK)
    return 0;
  else if ((mode & R_OK) && (file_mode & A_READABLE))
    return 0;
  else if ((mode & W_OK) && (file_mode & A_WRITABLE))
    return 0;
  else if ((mode & X_OK) && (file_mode & A_EXECABLE))
    return 0;

  return -1;
}

int Syscall::truncate(size_t path, off_t length) {
  debug(TRUNCATE, "Syscall::truncate called\n");
  if (path < PAGE_SIZE || path >= USER_BREAK)
    return -1;

  const char *file_path = (const char *)path;

  if (file_path == nullptr)
    return -1;

  int fd = FsOps::open(file_path, O_WRONLY);

  if (fd < 0)
    return -1;

  int return_value = Syscall::ftruncate(fd, length);

  FsOps::close(fd);
  return return_value;
}

int Syscall::ftruncate(int fd, off_t length) {
  debug(TRUNCATE, "Syscall::ftruncate called\n");
  if (!currentThread || currentThread->type_ != Thread::USER_THREAD)
    return -1;

  if (length < 0)
    return -1;

  const size_t max_size = static_cast<size_t>(static_cast<uint32>(-1));
  size_t new_size = static_cast<size_t>(length);
  if (new_size > max_size)
    return -1;

  UserThread *user_thread = static_cast<UserThread *>(currentThread);
  UserProcess *process = user_thread->process_;
  if (!process)
    return -1;

  FileSlotTable &local_fd_table = process->fileDescriptorTable();
  FileSlot *local_fd = local_fd_table.getLocalDescriptor(fd);
  if (!local_fd)
    return -1;

  if (local_fd->getType() == FileSlot::FileType::PIPE)
    return -1;

  if (local_fd->getType() == FileSlot::FileType::SHARED_MEMORY)
  {
    if ((local_fd->getPermission() & (O_WRONLY | O_RDWR)) == 0)
      return -1;
    ShMemSegment *obj =
        static_cast<ShMemSegment *>(local_fd->getKernelDescriptor());
    if (!obj)
      return -1;
    obj->setSize(new_size);
    return 0;
  }

  FileDescriptor *kernel_fd = local_fd->getKernelDescriptor();
  if (!kernel_fd)
    return -1;
  File *file = kernel_fd->getFile();
  if (!file)
    return -1;
  if ((file->getFlag() & (O_WRONLY | O_RDWR)) == 0)
    return -1;
  Inode *inode = file->getInode();
  if (!inode)
    return -1;
  return inode->truncate(new_size);
}

size_t Syscall::rename(size_t old_path_ptr, size_t new_path_ptr) {
  debug(RENAME, "Syscall::rename called\n");

  if (old_path_ptr < PAGE_SIZE || old_path_ptr >= USER_BREAK ||
      new_path_ptr < PAGE_SIZE || new_path_ptr >= USER_BREAK)
    return static_cast<size_t>(-1);

  const char *old_path = (const char *)old_path_ptr;
  const char *new_path = (const char *)new_path_ptr;

  if (old_path == nullptr || new_path == nullptr)
    return static_cast<size_t>(-1);

  int32 source_file = FsOps::open(old_path, O_RDONLY);
  int32 destination_file = FsOps::open(new_path, O_WRONLY | O_CREAT);

  if (source_file == -1) {
    debug(RENAME, "Syscall::rename: failed to open source file %s\n", old_path);
    return static_cast<size_t>(-1);
  }

  if (destination_file == -1) {
    debug(RENAME, "Syscall::rename: failed to open destination file %s\n",
          new_path);
    FsOps::close(source_file);
    return static_cast<size_t>(-1);
  }

  // transfer data
  char buffer[512];
  while (true) {
    int32 read = FsOps::read(static_cast<uint32>(source_file), buffer,
                                  sizeof(buffer));
    if (read == -1) {
      debug(RENAME, "Syscall::rename: read error on %s\n", old_path);
      FsOps::close(source_file);
      FsOps::close(destination_file);
      FsOps::rm(new_path);
      return static_cast<size_t>(-1);
    }

    if (read == 0)
      break;

    int32 write = FsOps::write(static_cast<uint32>(destination_file),
                                    buffer, static_cast<uint32>(read));
    if (write != read) {
      debug(RENAME, "Syscall::rename: write error on %s\n", new_path);
      FsOps::close(source_file);
      FsOps::close(destination_file);
      FsOps::rm(new_path);
      return static_cast<size_t>(-1);
    }
  }

  FsOps::close(source_file);
  FsOps::close(destination_file);

  // remove old file
  int32 rm = FsOps::rm(old_path);
  if (rm == -1) {
    debug(RENAME, "Syscall::rename: rm(%s) failed\n", old_path);
    return static_cast<size_t>(-1);
  }

  return 0;
}

int Syscall::dup(int file_descriptor_to_copy_id) {
  debug(DUP, "Syscall::dup syscall called\n");

  if (file_descriptor_to_copy_id < 0)
    return -1;

  UserProcess &userProces =
      *(UserProcess *)(static_cast<UserThread *>(currentThread)->process_);
  FileSlotTable &local_fd_table = userProces.fileDescriptorTable();

  FileSlot *file_descriptor_to_copy =
      local_fd_table.getLocalDescriptor(file_descriptor_to_copy_id);

  if (file_descriptor_to_copy == nullptr) {
    debug(DUP, "invalid file desriptor ID\n");
    return -1;
  }

  FileDescriptor *fd_object = file_descriptor_to_copy->getKernelDescriptor();
  FileSlot::FileType type = file_descriptor_to_copy->getType();
  uint32_t permission = file_descriptor_to_copy->getPermission();
  int new_file_desriptor =
      local_fd_table.createLocalDescriptor(fd_object, type, permission);

  return new_file_desriptor;
}

int Syscall::dup2(int old_fd, int new_fd) {
  debug(DUP, "Dup2 syscall called\n");
  if (old_fd < 0 || new_fd < 0)
    return -1;
  UserProcess &userProces =
      *(UserProcess *)(static_cast<UserThread *>(currentThread)->process_);
  FileSlotTable &local_fd_table = userProces.fileDescriptorTable();
  FileSlot *old_descriptor =
      local_fd_table.getLocalDescriptor(old_fd);
  if (!old_descriptor)
    return -1;
  if (old_fd == new_fd)
    return new_fd;
  if (local_fd_table.closeDescriptor(new_fd))
    userProces.onDescriptorClosed(new_fd);
  FileDescriptor *fd_object = old_descriptor->getKernelDescriptor();
  FileSlot::FileType type = old_descriptor->getType();
  uint32_t permission = old_descriptor->getPermission();
  if (!local_fd_table.assignDescriptor(new_fd, fd_object, type, permission))
    return -1;
  return new_fd;
}

int Syscall::pipe(int *file_descriptor_array) {

  // TODO check the size is 2
  if (file_descriptor_array == nullptr)
    return -1;

  PipeBuffer *buffer = new PipeBuffer();
  Pipe *read_endpoint = new Pipe(nullptr, buffer, Pipe::EndType::READ_END);
  Pipe *write_endpoint = new Pipe(nullptr, buffer, Pipe::EndType::WRITE_END);
  UserProcess &userProces =
      *(UserProcess *)(static_cast<UserThread *>(currentThread)->process_);
  FileSlotTable &local_fd_table = userProces.fileDescriptorTable();

  if (global_fd_list.add(static_cast<FileDescriptor *>(read_endpoint)) != 0) {
    delete read_endpoint;
    delete write_endpoint;
    return -1;
  }

  if (global_fd_list.add(static_cast<FileDescriptor *>(write_endpoint)) != 0) {
    global_fd_list.remove(static_cast<FileDescriptor *>(read_endpoint));
    delete read_endpoint;
    delete write_endpoint;
    return -1;
  }

  size_t read_fd = local_fd_table.createLocalDescriptor(
      static_cast<FileDescriptor *>(read_endpoint),
      FileSlot::FileType::PIPE, O_RDONLY);
  size_t write_fd = local_fd_table.createLocalDescriptor(
      static_cast<FileDescriptor *>(write_endpoint),
      FileSlot::FileType::PIPE, O_WRONLY);

  // FileSlot* localFileDescriptor =
  // local_fd_table.getLocalDescriptor(read_fd); debug(PIPE, "rest: \n");
  // debug(PIPE, "type is: %u\n",
  // static_cast<unsigned>(localFileDescriptor->getType()));
  debug(PIPE, "read_fd is: %lu\n", read_fd);
  debug(PIPE, "write_fd is: %lu\n", write_fd);
  file_descriptor_array[0] = read_fd;
  file_descriptor_array[1] = write_fd;

  return 0;
}

void Syscall::handle_pending_cancel(const char *location) {
  assert(currentThread &&
         "Syscall::syscallException: currentThread is null in handle_pending_"
         "cancel");

  // check if thread is a user thread
  // POC1: maybe replace this with just a return, other wise you might die in
  // very mysterious ways
  if (currentThread->type_ != Thread::USER_THREAD) {
    debug(SYSCALL, "Syscall::syscallException: thread is not a userthread\n");
    return;
  }

  UserThread *user_thread = static_cast<UserThread *>(currentThread);
  user_thread->cancel_statType_mutex_.acquire();

  // async cancel
  if (user_thread->getCancelType() == 1 &&
      user_thread->isThisThreadCanceled() &&
      user_thread->getCancelState() == 0) {
    user_thread->cancel_statType_mutex_.release();
    void *const pthread_canceled = (void *)PTHREAD_CANCELED;
    Syscall::pthread_exit(pthread_canceled);
    assert(false &&
           "Syscall::syscallException: pthread_exit returned unexpectedly");
  }

  if (!user_thread->isCanceled()) {
    user_thread->cancel_statType_mutex_.release();
    return;
  }

  if (user_thread->getCancelState() != 0) {
    user_thread->cancel_statType_mutex_.release();
    return;
  }

  user_thread->cancel_statType_mutex_.release();
  debug(SYSCALL,
        "Syscall::syscallException: thread %zu canceled at syscall %s\n",
        user_thread->getTID(), location);

  void *const pthread_canceled = (void *)PTHREAD_CANCELED;
  Syscall::pthread_exit(pthread_canceled);
  assert(false &&
         "Syscall::syscallException: pthread_exit returned unexpectedly");
}

void Syscall::pseudols(const char *pathname, char *buffer, size_t size) {
  if (buffer &&
      ((size_t)buffer >= USER_BREAK || (size_t)buffer + size > USER_BREAK))
    return;
  if ((size_t)pathname >= USER_BREAK)
    return;
  FsOps::readdir(pathname, buffer, size);
}

void Syscall::exit(size_t exit_code) {
  debug(SYSCALL, "Syscall::EXIT: called, exit_code: %zd\n", exit_code);

  // check if thread is a user thread
  if (currentThread->type_ != Thread::USER_THREAD) {
    debug(SYSCALL, "Syscall::exit is not a user thread, tid: %zu\n",
          currentThread->getTID());
    currentThread->kill();
  }

  UserThread *current_thread = (UserThread *)currentThread;
  UserProcess *process = current_thread->process_;

  // check if the syscall is allowed, or if it got already called
  process->process_state_lock_.acquire();

  // Disable Syscall while FORK is going on
  if (process->isThreadCreationPaused()) {
    debug(
        SYSCALL,
        "Syscall::exit: syscall not allowed, seems like fork is ongoing %zu\n",
        current_thread->getTID());
    process->process_state_lock_.release();
    return;
  }

  if (process->isThreadCreationDisabled()) {
    debug(SYSCALL,
          "Syscall::exit: syscall not allowed, seems like exit got already "
          "called %zu\n",
          current_thread->getTID());

    // Thread could have a segfault while creating a new thread
    if (current_thread->is_creating_new_thread_) {
      debug(SYSCALL,
            "Syscall::exit: thread is creating a new thread, decreasing "
            "creation counter %zu\n",
            current_thread->getTID());
      process->decreaseThreadCreationCounter();
    }

    process->process_state_lock_.release();
    // should be killed anyways
    current_thread->kill();
    assert(false && "Thread should be dead what happened");
    return;
  }
  assert(process->process_state_ == PROCESS_STATE::P_RUNNING &&
         "Process State is not P_RUNNING");
  process->process_state_ = PROCESS_STATE::P_EXIT;
  process->process_state_condition_.broadcast();
  process->process_state_lock_.release();

  // Before we start the killing of all threads, we need to make sure we are not
  // currently creating any new threads
  process->waitUntilAllThreadsAreCreated();

  // Mark all the threads so they can be killed later on
  process->markAllThreadsToKilledExceptCalling();

  // Wait till all threads are dead except the calling thread -> the thread that
  // called exit
  process->waitUntilAllThreadsAreDeadExceptCalling();

  // To be posix compliant this is only allowed when the process is not
  //  running anymore -> last thread alive and calling thread of exit calls this
  process->setExitStatus(static_cast<int>(exit_code));
  if (AppRegistry *registry = AppRegistry::instance())
    registry->notifyProcessExit(process, static_cast<int>(exit_code));

  // If we all this it has to be the last thread running in the process
  pthread_exit(reinterpret_cast<void *>(exit_code));
  assert(false && "This should never happen");
}

size_t Syscall::write(size_t fd, pointer buffer, size_t size) {
  // WARNING: this might fail if Kernel PageFaults are not handled
  if ((buffer >= USER_BREAK) || (buffer + size > USER_BREAK)) {
    return -1U;
  }

  UserProcess &userProces =
      *(UserProcess *)(static_cast<UserThread *>(currentThread)->process_);
  FileSlotTable &local_fd_table = userProces.fileDescriptorTable();
  FileSlot *localFileDescriptor =
      local_fd_table.getLocalDescriptor(fd);

  size_t num_written = 0;

  if (localFileDescriptor != nullptr &&
      localFileDescriptor->getType() == FileSlot::FileType::PIPE) {
    FileDescriptor *fd_object = localFileDescriptor->getKernelDescriptor();
    Pipe *pipe = static_cast<Pipe *>(fd_object);
    num_written = pipe->write((char *)buffer, size);
    if (num_written == -1UL) {
      return static_cast<size_t>(-1);
    }
  } else if (fd == fd_stdout) // stdout
  {
    debug(SYSCALL, "Syscall::write: %.*s\n", (int)size, (char *)buffer);
    kprintf("%.*s", (int)size, (char *)buffer);
    num_written = size;
  } else {
    if (!fd_visible_to_current_process(fd)) {
      return static_cast<size_t>(-1);
    }
    num_written = FsOps::write(fd, (char *)buffer, size);
  }
  return num_written;
}

size_t Syscall::read(size_t fd, pointer buffer, size_t count) {
  if ((buffer >= USER_BREAK) || (buffer + count > USER_BREAK)) {
    return -1U;
  }

  UserProcess &userProces =
      *(UserProcess *)(static_cast<UserThread *>(currentThread)->process_);
  FileSlotTable &local_fd_table = userProces.fileDescriptorTable();
  FileSlot *localFileDescriptor =
      local_fd_table.getLocalDescriptor(fd);

  size_t num_read = 0;

  if (localFileDescriptor != nullptr &&
      localFileDescriptor->getType() == FileSlot::FileType::PIPE) {
    FileDescriptor *fd_object = localFileDescriptor->getKernelDescriptor();
    Pipe *pipe = static_cast<Pipe *>(fd_object);
    num_read = pipe->read((char *)buffer, count);
  } else if (fd == fd_stdin) {
    // this doesn't! terminate a string with \0, gotta do that yourself
    num_read = currentThread->getTerminal()->readLine((char *)buffer, count);
    debug(SYSCALL, "Syscall::read: %.*s\n", (int)num_read, (char *)buffer);
  } else {
    if (!fd_visible_to_current_process(fd)) {
      return static_cast<size_t>(-1);
    }
    num_read = FsOps::read(fd, (char *)buffer, count);
  }
  return num_read;
}

size_t Syscall::lseek(size_t fd, l_off_t offset, size_t whence) {
  if (whence > 2) {
    return static_cast<size_t>(-1);
  }

  if (!fd_visible_to_current_process(fd)) {
    return static_cast<size_t>(-1);
  }

  l_off_t result = FsOps::lseek(static_cast<uint32>(fd), offset,
                                     static_cast<uint8>(whence));
  return static_cast<size_t>(result);
}

size_t Syscall::close(size_t fd) {
  UserProcess &userProces =
      *(UserProcess *)(static_cast<UserThread *>(currentThread)->process_);
  FileSlotTable &local_fd_table = userProces.fileDescriptorTable();
  FileSlot *localFileDescriptor =
      local_fd_table.getLocalDescriptor(fd);

  if (localFileDescriptor != nullptr &&
      localFileDescriptor->getType() == FileSlot::FileType::PIPE) {
    if (local_fd_table.closeDescriptor(static_cast<int32>(fd))) {
      userProces.onDescriptorClosed(static_cast<int32>(fd));
      return 0;
    }
    return static_cast<size_t>(-1);
  } else if (currentThread && currentThread->type_ == Thread::USER_THREAD) {
    UserThread *user_thread = static_cast<UserThread *>(currentThread);
    if (UserProcess *process = user_thread->process_) {
      if (process->fileDescriptorTable().closeDescriptor(
              static_cast<int32>(fd))) {
        process->onDescriptorClosed(static_cast<int32>(fd));
        return 0;
      }
      return static_cast<size_t>(-1);
    }
  }

  return static_cast<size_t>(FsOps::close(fd));
}

size_t Syscall::open(size_t path, size_t flags) {
  if (path < PAGE_SIZE || path >= USER_BREAK) {
    return -1U;
  }
  if (path == 0) {
    return -1U;
  }
  int32 fd = FsOps::open((char *)path, flags);
  if (fd < 0)
    return static_cast<size_t>(-1);

  if (currentThread && currentThread->type_ == Thread::USER_THREAD) {
    UserThread *user_thread = static_cast<UserThread *>(currentThread);
    if (UserProcess *process = user_thread->process_) {
      if (!process->fileDescriptorTable().registerDescriptor(fd)) {
        debug(SYSCALL, "Syscall::open: registerDescriptor failed for fd %d\n",
              fd);
        FsOps::close(fd);
        return static_cast<size_t>(-1);
      }
    }
  }

  return static_cast<size_t>(fd);
}

void Syscall::outline(size_t port, pointer text) {
  // WARNING: this might fail if Kernel PageFaults are not handled
  if (text >= USER_BREAK) {
    return;
  }
  if (port == 0xe9) // debug port
  {
    writeLineDebug((const char *)text);
  }
}

size_t Syscall::createprocess(size_t path, size_t sleep) {
  // THIS METHOD IS FOR TESTING PURPOSES ONLY AND NOT MULTITHREADING SAFE!
  // AVOID USING IT AS SOON AS YOU HAVE AN ALTERNATIVE!

  // parameter check begin
  if (path >= USER_BREAK) {
    return -1U;
  }

  debug(SYSCALL, "Syscall::createprocess: path:%s sleep:%zd\n", (char *)path,
        sleep);
  ssize_t fd = FsOps::open((const char *)path, O_RDONLY);
  if (fd == -1) {
    return -1U;
  }
  FsOps::close(fd);
  // parameter check end

  size_t process_count = AppRegistry::instance()->processCount();
  AppRegistry::instance()->createProcess((const char *)path);
  if (sleep) {
    while (AppRegistry::instance()->processCount() >
           process_count) // please note that this will fail ;)
    {
      Scheduler::instance()->yield();
    }
  }
  return 0;
}

size_t Syscall::fork() {
  debug(SYSCALL, "Syscall::fork called\n");

  // TODO: Look at Exit and make sure fork is safe with it
  // also if Exit already got called disable fork

  // Fork only makes sense for user threads; kernel threads cannot fork.
  if (!currentThread || currentThread->type_ != Thread::USER_THREAD)
    return static_cast<size_t>(-1);

  UserThread *parent_thread = static_cast<UserThread *>(currentThread);
  UserProcess *parent_process = parent_thread->process_;
  if (!parent_process)
    return static_cast<size_t>(-1);
  // abood did that
  parent_process->process_state_lock_.acquire();

  // Disable fork if the process has already exited or the calling thread is
  // canceled
  if (parent_process->hasExited() || parent_thread->isCanceled()) {
    parent_process->process_state_lock_.release();
    return static_cast<size_t>(-1);
  }

  // Wait for any ongoing fork to complete so concurrent forks serialize.
  while (parent_process->process_state_ == PROCESS_STATE::P_FORK) {
    parent_process->process_state_condition_.wait();
  }

  // Disable fork while thread creation is paused or disabled (exit ongoing)
  // paused -> fork ongoing / disabled -> exit ongoing
  // POC1: a fork call should not fail, just because there is another fork call
  // ongoing
  // if (parent_process->isThreadCreationPaused() ||
  //     parent_process->isThreadCreationDisabled()) {
  //   parent_process->process_state_lock_.release();
  //   return static_cast<size_t>(-1);
  // }

  if (parent_process->process_state_ != PROCESS_STATE::P_RUNNING) {
    parent_process->process_state_lock_.release();
    return static_cast<size_t>(-1);
  }
  parent_process->process_state_ = PROCESS_STATE::P_FORK;
  parent_process->process_state_condition_.broadcast();
  parent_process->process_state_lock_.release();

  // Prevent cloning while new threads are being added to the process.
  parent_process->waitUntilAllThreadsAreCreated();
  // end abood
  // Clone the parent process. The UserProcess constructor performs all the
  // heavy lifting: duplicating address space, cloning the thread state, and
  // registering the child with the process registry.
  UserProcess *child_process = new UserProcess(parent_process, parent_thread);

  // abood did that
  parent_process->process_state_lock_.acquire();
  parent_process->process_state_ = PROCESS_STATE::P_RUNNING;
  parent_process->process_state_condition_.broadcast();
  parent_process->process_state_lock_.release();
  // end abood

  if (!child_process || !child_process->init_success_) {
    if (child_process)
      delete child_process;
    return static_cast<size_t>(-1);
  }

  debug(SYSCALL, "Syscall::fork: returning child PID %zu\n",
        child_process->getPID());
  // end abood
  return child_process->getPID();
}

size_t Syscall::waitpid(size_t pid, int *status, int options) {
  if (options != 0)
    return static_cast<size_t>(-1);

  if (status) {
    size_t addr = reinterpret_cast<size_t>(status);
    if (addr >= USER_BREAK || USER_BREAK - addr < sizeof(int))
      return static_cast<size_t>(-1);
  }

  // waitpid only applies to user threads
  if (!currentThread || currentThread->type_ != Thread::USER_THREAD)
    return static_cast<size_t>(-1);

  UserThread *current_thread = static_cast<UserThread *>(currentThread);
  UserProcess *process = current_thread->process_;
  if (!process)
    return static_cast<size_t>(-1);

  AppRegistry *registry = AppRegistry::instance();
  if (!registry)
    return static_cast<size_t>(-1);

  // only allow waiting for an explicit pid.
  long child_pid = static_cast<long>(static_cast<intptr_t>(pid));
  if (child_pid < 0)
    return static_cast<size_t>(-1);

  int collected = 0;
  size_t waited_pid = registry->waitForProcess(static_cast<size_t>(child_pid),
                                               status ? &collected : nullptr);

  if (waited_pid == static_cast<size_t>(-1))
    return static_cast<size_t>(-1);

  if (status)
    *status = collected;

  return waited_pid;
}

void Syscall::trace() { currentThread->printBacktrace(); }

void Syscall::pthread_exit(void *value_ptr) {
  // debug(SYSCALL, "============= pthread_exit: called ============\n");

  UserThread *current_thread = (UserThread *)currentThread;
  UserProcess *process = current_thread->process_;

  {
    // ScopeLock process_lock(process->thread_lock_);
    process->thread_exit_mutex.acquire();
    if (!current_thread->isDetached()) {
      process->threads_ret_val[current_thread->getTID()] = value_ptr;
    } else {
      process->threads_ret_val.erase(current_thread->getTID());
    }
    process->thread_exit_condition.broadcast();
    process->thread_exit_mutex.release();
  }

  current_thread->kill();
  assert(false && "This should never happen");
}

size_t Syscall::pthread_cancel(size_t thread) {
  debug(SYSCALL, "Syscall::pthread_cancel: target thread %zu\n", thread);

  UserThread *current_thread = (UserThread *)currentThread;
  if (!current_thread)
    return ESRCH;

  UserProcess *process = current_thread->process_;
  if (!process)
    return ESRCH;

  ScopeLock process_lock(process->thread_lock_);

  UserThread *target_thread = nullptr;
  if (thread == current_thread->getTID()) {
    target_thread = current_thread;
  } else {
    target_thread = process->getThreadById(thread);
  }

  if (!target_thread) {
    debug(SYSCALL,
          "Syscall::pthread_cancel: thread %zu not found (ESRCH equivalent)\n",
          thread);
    return ESRCH;
  }

  target_thread->cancel_statType_mutex_.acquire();
  target_thread->setThreadAsCanceled();
  target_thread->cancel_statType_mutex_.release();

  return 0;
}

size_t Syscall::pthread_detach(size_t thread) {
  debug(SYSCALL, "Syscall::pthread_detach: caller %zu detaching %zu\n",
        currentThread ? currentThread->getTID() : 0, thread);

  UserThread *current_thread = static_cast<UserThread *>(currentThread);
  if (!current_thread)
    return ESRCH;

  UserProcess *process = current_thread->process_;
  if (!process)
    return ESRCH;

  ScopeLock process_lock(process->thread_lock_);

  UserThread *target_thread = nullptr;
  if (thread == current_thread->getTID()) {
    target_thread = current_thread;
  } else {
    target_thread = process->getThreadById(thread);
  }

  process->thread_exit_mutex.acquire();
  auto ret_it = process->threads_ret_val.find(thread);

  if (!target_thread) {
    if (ret_it != process->threads_ret_val.end()) {
      debug(SYSCALL,
            "Syscall::pthread_detach: thread %zu already exited, cleaning up\n",
            thread);
      process->threads_ret_val.erase(ret_it);
      process->thread_exit_condition.broadcast();
      process->thread_exit_mutex.release();
      return 0;
    }

    process->thread_exit_mutex.release();
    debug(SYSCALL,
          "Syscall::pthread_detach: thread %zu not found (ESRCH equivalent)\n",
          thread);
    return ESRCH;
  }

  if (target_thread->isDetached()) {
    process->thread_exit_mutex.release();
    debug(SYSCALL,
          "Syscall::pthread_detach: thread %zu already detached (EINVAL "
          "equivalent)\n",
          thread);
    return EINVAL;
  }

  target_thread->setDetached();

  if (ret_it != process->threads_ret_val.end()) {
    process->threads_ret_val.erase(ret_it);
  }

  process->thread_exit_condition.broadcast();
  process->thread_exit_mutex.release();

  return 0;
}

// POC1: what if the thread had cancelling disabled, but a cancellation request
// pending, and now cancel gets enabled ;)
size_t Syscall::pthread_setcancelstate(int state, int *oldstate) {
  constexpr int PTHREAD_CANCEL_ENABLE = 0;
  constexpr int PTHREAD_CANCEL_DISABLE = 1;

  if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE)
    return EINVAL;

  if (oldstate) {
    size_t addr = reinterpret_cast<size_t>(oldstate);
    if (addr >= USER_BREAK || USER_BREAK - addr < sizeof(int))
      return EFAULT;
  }

  UserThread *current_thread = static_cast<UserThread *>(currentThread);
  if (!current_thread)
    return ESRCH;

  current_thread->cancel_statType_mutex_.acquire();
  int previous = current_thread->getCancelState();
  current_thread->setCancelState(state);
  bool enable_pending_cancel =
      (state == PTHREAD_CANCEL_ENABLE && previous == PTHREAD_CANCEL_DISABLE &&
       current_thread->isCanceled());
  current_thread->cancel_statType_mutex_.release();

  if (oldstate)
    *oldstate = previous;

  if (enable_pending_cancel)
    Syscall::handle_pending_cancel("pthread_setcancelstate");

  return 0;
}

// POC1: Almost same thing as above
size_t Syscall::pthread_setcanceltype(int type, int *oldtype) {
  constexpr int PTHREAD_CANCEL_DEFERRED = 0;
  constexpr int PTHREAD_CANCEL_ENABLE = 0;
  constexpr int PTHREAD_CANCEL_ASYNCHRONOUS = 1;

  if (type != PTHREAD_CANCEL_DEFERRED && type != PTHREAD_CANCEL_ASYNCHRONOUS)
    return EINVAL;

  if (oldtype) {
    size_t addr = reinterpret_cast<size_t>(oldtype);
    if (addr >= USER_BREAK || USER_BREAK - addr < sizeof(int))
      return EFAULT;
  }

  UserThread *current_thread = static_cast<UserThread *>(currentThread);
  if (!current_thread)
    return ESRCH;
  debug(SYSCALL,
        "Syscall::pthread_setcanceltype: START thread %zu setting type %d\n",
        current_thread->getTID(), type);
  current_thread->cancel_statType_mutex_.acquire();
  int previous = current_thread->getCancelType();
  current_thread->setCancelType(type);
  bool trigger_pending_cancel =
      (type == PTHREAD_CANCEL_ASYNCHRONOUS &&
       previous == PTHREAD_CANCEL_DEFERRED && current_thread->isCanceled() &&
       current_thread->getCancelState() == PTHREAD_CANCEL_ENABLE);

  current_thread->cancel_statType_mutex_.release();

  if (oldtype)
    *oldtype = previous;

  if (trigger_pending_cancel)
    Syscall::handle_pending_cancel("pthread_setcanceltype");

  debug(SYSCALL,
        "Syscall::pthread_setcanceltype: END thread %zu previous type %d\n",
        current_thread->getTID(), previous);
  return 0;
}

size_t Syscall::pthread_join(size_t thread, void **value_ptr) {
  // debug(SYSCALL, "Syscall::pthread_join: caller %zu waiting on %zu\n",
  //       currentThread ? currentThread->getTID() : 0, thread);

  // POC1: you need to check for the whole null-page
  if (value_ptr) {
    size_t addr = (size_t)value_ptr;
    if (addr < PAGE_SIZE || addr >= USER_BREAK ||
        USER_BREAK - addr < sizeof(void *)) {
      debug(SYSCALL,
            "Syscall::pthread_join: invalid value_ptr %p (outside userspace)\n",
            value_ptr);
      return EFAULT;
    }
  }

  UserThread *current_thread = static_cast<UserThread *>(currentThread);
  if (!current_thread)
    return ESRCH;

  if (thread == current_thread->getTID()) {
    debug(SYSCALL,
          "Syscall::pthread_join: thread %zu attempted to join itself\n",
          thread);
    return EDEADLK;
  }

  UserProcess *process = current_thread->process_;
  if (!process)
    return ESRCH;

  process->thread_lock_.acquire();
  process->thread_exit_mutex.acquire();
  process->thread_join_dependencies.erase(current_thread->getTID());
  bool waiting_registered = false;

  void *result = nullptr;
  while (true) {
    // debug(SYSCALL, "Syscall::pthread_join: checking thread %zu\n", thread);
    auto ret_it = process->threads_ret_val.find(thread);
    if (ret_it != process->threads_ret_val.end()) {
      // debug(SYSCALL, "Syscall::pthread_join: thread %zu has exited\n",
      // thread);
      result = ret_it->second;
      process->threads_ret_val.erase(ret_it);
      if (waiting_registered) {
        process->thread_join_dependencies.erase(current_thread->getTID());
        waiting_registered = false;
      }
      process->thread_exit_mutex.release();
      process->thread_lock_.release();
      break;
    }

    UserThread *target_thread = process->getThreadById(thread);
    if (target_thread && target_thread->isDetached()) {
      if (waiting_registered) {
        process->thread_join_dependencies.erase(current_thread->getTID());
        waiting_registered = false;
      }
      process->thread_exit_mutex.release();
      process->thread_lock_.release();
      // debug(
      //     SYSCALL,
      //     "Syscall::pthread_join: thread %zu is detached (EINVAL
      //     equivalent)\n", thread);
      return EINVAL;
    }

    if (!target_thread) {
      if (waiting_registered) {
        process->thread_join_dependencies.erase(current_thread->getTID());
        waiting_registered = false;
      }
      process->thread_exit_mutex.release();
      process->thread_lock_.release();
      debug(SYSCALL,
            "Syscall::pthread_join: thread %zu not found or already joined\n",
            thread);
      return ESRCH;
    }

    if (!waiting_registered) {
      // Follow the existing join dependencies to check if we would create a
      // loop.
      bool cycle_found = false;
      size_t check_tid = thread;
      for (size_t steps = 0; steps < MAX_THREADS_PER_PROCESS; ++steps) {
        if (check_tid == current_thread->getTID()) {
          cycle_found = true;
          break;
        }
        auto dep_it = process->thread_join_dependencies.find(check_tid);
        if (dep_it == process->thread_join_dependencies.end())
          break;
        check_tid = dep_it->second;
        if (check_tid == thread) {
          cycle_found = true;
          break;
        }
      }

      if (cycle_found) {
        debug(SYSCALL,
              "Syscall::pthread_join: circular join detected involving %zu\n",
              thread);
        process->thread_exit_mutex.release();
        process->thread_lock_.release();
        return EDEADLK;
      }
      process->thread_join_dependencies[current_thread->getTID()] = thread;
      waiting_registered = true;
    }

    debug(SYSCALL,
          "Syscall::pthread_join: waiting for thread %zu name: %s to exit\n",
          thread, target_thread->getName());
    process->thread_lock_.release();
    process->thread_exit_condition.wait(false);
    // Thread may have been canceled while waiting; check outside locks to avoid
    // deadlocks inside pthread_exit.
    Syscall::handle_pending_cancel("pthread_join-wait");
    process->thread_lock_.acquire();
    process->thread_exit_mutex.acquire();
  }

  // POC1: only one thread should get the return value
  if (value_ptr)
    *value_ptr = result;

  // debug(SYSCALL,
  //       "Syscall::pthread_join: thread %zu collected return value %p\n",
  //       thread, result);
  return 0;
}

uint32 Syscall::get_thread_count() {
  return Scheduler::instance()->getThreadCount();
}

void Syscall::boot_crash_test() { assert(false && "Syscall::boot_crash_test called"); }

size_t Syscall::sleep(size_t seconds) {
  debug(SYSCALL, "Syscall::sleep: %zd seconds\n", seconds);

  if (seconds == 0)
    return 0;

  size_t elapsed_time =
      ((UserThread *)currentThread)->sendThreadToSleep(seconds * 1000);

  if (elapsed_time > 0)
    return elapsed_time;

  // If sleep() returns because the requested time has elapsed, the value
  // returned shall be 0
  return 0;
}

size_t Syscall::usleep(size_t microseconds) {
  debug(SYSCALL, "Syscall::usleep: %zd microseconds\n", microseconds);

  if (microseconds == 0)
    return 0;

  size_t ms = microseconds / 1000;

  if (ms == 0)
    ms = 1;

  size_t elapsed_time = ((UserThread *)currentThread)->sendThreadToSleep(ms);

  if (elapsed_time > 0)
    return elapsed_time;

  // If usleep() returns because the requested time has elapsed, the value
  // returned shall be 0
  return 0;
}

size_t Syscall::pthread_create(size_t *thread, size_t pthread_attr_t,
                               void *(*start_routine)(void *), void *arg,
                               size_t wrapper) {
  debug(SYSCALL, "Syscall::pthread_create: called\n");

  if ((size_t)start_routine < PAGE_SIZE || (size_t)thread < PAGE_SIZE ||
      (size_t)thread >= USER_BREAK || pthread_attr_t >= USER_BREAK ||
      (size_t)start_routine >= USER_BREAK || (size_t)arg >= USER_BREAK ||
      start_routine == nullptr || thread == nullptr || wrapper >= USER_BREAK) {
    debug(SYSCALL,
          "Syscall::pthread_create: invalid pointer argument(s) passed\n");
    return EINVAL;
  }

  UserThread *current_thread = static_cast<UserThread *>(currentThread);
  if (!current_thread)
    return ESRCH;

  UserProcess *process = current_thread->process_;
  if (!process)
    return ESRCH;
  // ScopeLock process_lock(process->thread_lock_);
  size_t ret_val = process->addThread(thread, pthread_attr_t, start_routine,
                                      arg, wrapper, false);

  return ret_val;
}

size_t Syscall::clock() {
  debug(SYSCALL, "Syscall::time: called\n");

  return (size_t)((UserThread *)currentThread)->process_->getClockCyclesRun();
}

size_t Syscall::pthread_self() {
  UserThread *current_thread = (UserThread *)currentThread;
  return current_thread->getTID();
}

size_t Syscall::execv(const char *path, char *const argv[]) {
  debug(SYSCALL, "Syscall::execv: called with path %s\n", path);

  if ((size_t)path >= USER_BREAK || path == nullptr ||
      (size_t)path < PAGE_SIZE || (size_t)argv >= USER_BREAK) {
    debug(SYSCALL, "Syscall::execv: param invalid\n");
    return -1;
  }

  UserThread *current_thread = (UserThread *)currentThread;
  UserProcess *process = current_thread->process_;

  process->process_state_lock_.acquire();
  // Disable Syscall while FORK is going on
  if (process->isThreadCreationPaused()) {
    debug(
        SYSCALL,
        "Syscall::exec: syscall not allowed, seems like fork is ongoing %zu\n",
        current_thread->getTID());
    process->process_state_lock_.release();
    return -1;
  }

  // Wait here if exec fails, because then the waiting one can try
  while (process->isThreadCreationPaused()) {
    process->process_state_condition_.wait();
    if (process->isThreadCreationDisabled()) {
      process->process_state_lock_.release();
      debug(SYSCALL,
            "Another Thread is now executing EXEC killing this thread %s | "
            "TID: %ld\n",
            process->minixfs_filename_.c_str(), current_thread->getTID());
      current_thread->kill();
    }
  }

  assert(process->process_state_ == PROCESS_STATE::P_RUNNING &&
         "Process State is not P_RUNNING");
  process->process_state_ = PROCESS_STATE::P_EXEC;
  process->process_state_condition_.broadcast();
  process->process_state_lock_.release();

  size_t ret_val =
      ((UserThread *)currentThread)->process_->handleExecProcess(path, argv);

  return ret_val;
}

size_t Syscall::brk(void *addr) {
  debug(SYSCALL, "Syscall::brk: called with addr %p\n", (void *)addr);

  if ((size_t)addr >= USER_BREAK || addr == nullptr) {
    return -1;
  }

  UserThread *current_thread = (UserThread *)currentThread;
  UserProcess *process = current_thread->process_;

  return process->heap_manager_->setHeapEnd(addr) ? 0 : (size_t)-1;
}

size_t Syscall::sbrk(size_t increment) {
  debug(SYSCALL, "Syscall::sbrk: called with increment %zd\n", increment);
  UserThread *current_thread = (UserThread *)currentThread;
  UserProcess *process = current_thread->process_;

  return (size_t)process->heap_manager_->incHeapBy(increment);
}

void *Syscall::mmap(void *start, size_t length, int prot, int flags, int fd,
                    off_t offset) {
  (void)start;
  if (!currentThread || currentThread->type_ != Thread::USER_THREAD) {
    return MAP_FAILED;
  }

  UserThread *user_thread = static_cast<UserThread *>(currentThread);
  UserProcess *process = user_thread->process_;
  if (!process || !process->sharedMemManager()) {
    return MAP_FAILED;
  }

  ShMemRegistry *manager = process->sharedMemManager();
  if (!manager) {
    return MAP_FAILED;
  }
  return manager->mmap(nullptr, length, prot, flags, fd, offset);
}

int Syscall::munmap(void *start, size_t length) {
  if (!currentThread || currentThread->type_ != Thread::USER_THREAD) {
    return -1;
  }
  UserThread *user_thread = static_cast<UserThread *>(currentThread);
  UserProcess *process = user_thread->process_;
  if (!process || !process->sharedMemManager()) {
    return -1;
  }
  ShMemRegistry *manager = process->sharedMemManager();
  if (!manager) {
    return -1;
  }
  return manager->munmap(start, length);
}

int Syscall::mprotect(void *addr, size_t len, int prot) {
  if (!currentThread || currentThread->type_ != Thread::USER_THREAD) {
    return -1;
  }

  if (!addr || len == 0) {
    return -1;
  }

  if ((size_t)addr % PAGE_SIZE) {
    return -1;
  }

  const int allowed_prot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_NONE;
  if (prot & ~allowed_prot) {
    return -1;
  }

  size_t start = (size_t)addr;
  size_t end = start + len;
  if (end < start || start < PAGE_SIZE || end > USER_BREAK) {
    return -1;
  }

  UserThread *user_thread = static_cast<UserThread *>(currentThread);
  UserProcess *process = user_thread->process_;
  if (!process || !process->loader_) {
    return -1;
  }

  bool fully_shared_range =
      start >= MIN_SHARED_MEM_ADDRESS && end <= MAX_SHARED_MEM_ADDRESS;
  if (fully_shared_range) {
    ShMemRegistry *manager = process->sharedMemManager();
    if (!manager) {
      return -1;
    }
    return manager->mprotect(addr, len, prot);
  }

  if (start < MAX_SHARED_MEM_ADDRESS && end > MIN_SHARED_MEM_ADDRESS) {
    return -1;
  }

  size_t start_vpn = start / PAGE_SIZE;
  size_t num_pages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
  ArchMemory &arch_memory = process->loader_->arch_memory_;

  arch_memory.arch_memory_lock_.acquire();
  for (size_t i = 0; i < num_pages; ++i) {
    size_t vpn = start_vpn + i;
    ArchMemoryMapping mapping = arch_memory.resolveMapping(vpn);
    if (!mapping.pt) {
      arch_memory.arch_memory_lock_.release();
      return -1;
    }
    PageTableEntry &pte = mapping.pt[mapping.pti];
    if (!pte.present && !pte.swapped_out) {
      arch_memory.arch_memory_lock_.release();
      return -1;
    }
    bool want_write = (prot & PROT_WRITE) != 0;
    pte.writeable = want_write ? 1 : 0;
    pte.execution_disabled = (prot & PROT_EXEC) ? 0 : 1;
  }
  arch_memory.arch_memory_lock_.release();
  ArchMemory::flushTlb();

  return 0;
}

int Syscall::shm_open(const char *name, int oflag, mode_t mode) {
  if (!name) {
    return -1;
  }
  size_t name_ptr = reinterpret_cast<size_t>(name);
  if (name_ptr < PAGE_SIZE || name_ptr >= USER_BREAK) {
    return -1;
  }
  if (!currentThread || currentThread->type_ != Thread::USER_THREAD) {
    return -1;
  }
  UserThread *user_thread = static_cast<UserThread *>(currentThread);
  UserProcess *process = user_thread->process_;
  if (!process) {
    return -1;
  }
  return ShMemRegistry::shm_open(name, oflag, mode, process);
}

int Syscall::shm_unlink(const char *name) {
  if (!name) {
    return -1;
  }
  size_t name_ptr = reinterpret_cast<size_t>(name);
  if (name_ptr < PAGE_SIZE || name_ptr >= USER_BREAK) {
    return -1;
  }
  return ShMemRegistry::shm_unlink(name);
}
size_t Syscall::set_replacement_algo_func(size_t pra) {
  debug(SYSCALL, "Syscall::set_replacement_algo: called with pra %zd\n", pra);

  switch (pra) {
  case 0:
    SwapEngine::instance()->setCurrentPRA(PageReplacementAlgorithm::RANDOM);
    break;
  case 1:
    SwapEngine::instance()->setCurrentPRA(PageReplacementAlgorithm::AGING);
    break;
  case 2:
    SwapEngine::instance()->setCurrentPRA(
        PageReplacementAlgorithm::SECOND_CHANCE);
    break;
  default:
    return -1;
  }

  return 0;
}
