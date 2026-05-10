
#include "StackAllocator.h"
#include "ArchMemory.h"
#include "ArchThreads.h"
#include "FrameAlloc.h"
#include "Scheduler.h"
#include "UserProcess.h"
#include "UserThread.h"
#include "debug.h"
#include "kstring.h"
#include "offsets.h"
#include "paging-definitions.h"
#include "kernel_user_shared.h"
#include "uios.h"
#include "uutility.h"
#include <assert.h>

namespace {
inline size_t alignUp(size_t value, size_t alignment) {
  if (alignment <= 1)
    return value;
  return (value + alignment - 1) & ~(alignment - 1);
}
} // namespace

StackAllocator::StackAllocator(UserProcess *process)
    : stack_info_lock_("StackAllocator::stack_info_lock_") {
  debug(THREADS_STACK_MANAGER, "StackAllocator ctor\n");

  exec_param_pointer_page_ppn_ = 0;
  exec_param_page_ppn_ = 0;
  exec_param_page_allocated_ = false;
  exec_param_pointer_page_allocated_ = false;
  argv_count_ = 0;
  process_ = process;
  exec_param_pointer_page_vpn_ = USER_BREAK / PAGE_SIZE - 1;

  if (THREAD_STACK_ASLR) {
    size_t aslr_offset =
        Scheduler::instance()->getRandomNumber(MAX_STACK_ASLR_OFFSET);
    exec_param_pointer_page_vpn_ -= aslr_offset;
    debug(
        THREADS_STACK_MANAGER,
        "Applying ASLR offset of %zu pages to thread stacks for process %ld\n",
        aslr_offset, process_->getPID());
  }

  exec_param_page_vpn_ = exec_param_pointer_page_vpn_ - 1;
  first_vpn_ = exec_param_page_vpn_ - 1; // is the first guard page

  debug(THREADS_STACK_MANAGER, "StackAllocator ctor done\n");
}

StackAllocator::StackAllocator(StackAllocator *other)
    : stack_info_lock_("StackAllocator::stack_info_lock_") {
  debug(THREADS_STACK_MANAGER, "StackAllocator copy ctor\n");

  process_ = nullptr;
  exec_param_pointer_page_vpn_ = other->exec_param_pointer_page_vpn_;
  exec_param_page_vpn_ = other->exec_param_page_vpn_;
  first_vpn_ = other->first_vpn_;
  exec_param_page_ppn_ = other->exec_param_page_ppn_;
  exec_param_pointer_page_ppn_ = other->exec_param_pointer_page_ppn_;
  argv_count_ = other->argv_count_;
  exec_param_page_allocated_ = false;
  exec_param_pointer_page_allocated_ = false;
}

StackAllocator::~StackAllocator() {
  debug(THREADS_STACK_MANAGER, "StackAllocator dtor\n");
  resetThreadStacks();
  debug(THREADS_STACK_MANAGER, "StackAllocator dtor done\n");
}

size_t StackAllocator::getExecParamPageVPN() {
  return exec_param_page_vpn_;
}

size_t StackAllocator::vpnToPageStartAddress(size_t vpn) {
  return (vpn * PAGE_SIZE) + PAGE_SIZE;
}

size_t StackAllocator::vpnToPageEndAddress(size_t vpn) {
  return ((vpn + 1) * PAGE_SIZE) - 1;
}

size_t StackAllocator::getStackRegionStartVPN(size_t thread_id) {
  return first_vpn_ - (thread_id * (MAX_THREAD_STACKS + GUARD_PAGE));
}

size_t StackAllocator::getNextGuardPageVPN(size_t thread_id) {
  return getStackRegionStartVPN(thread_id) - GUARD_PAGE - MAX_THREAD_STACKS;
}

size_t StackAllocator::getStackStartVPN(size_t thread_id) {
  return getStackRegionStartVPN(thread_id) - GUARD_PAGE;
}

size_t StackAllocator::addressToVPN(size_t address) {
  return address / PAGE_SIZE;
}

size_t StackAllocator::vpnToThreadID(size_t vpn) {
  return (first_vpn_ - vpn) / (MAX_THREAD_STACKS + GUARD_PAGE);
}

/*
   Stack Region Layout
   -------------------

   +------------------------+  <-- Here is the array of pointers to argv
   |EXEC Param Pointer Page |
   +------------------------+  <-- getExecParamPageVPN () | One per process
   |       EXEC Param Page  |
   +------------------------+  <-- StackRegionStartVPN()
   |       Guard Page 1     |
   +------------------------+  <-- stackStartVPN() for Thread 0
   |   pthread_thread_info  |
   |          -----         |
   |       Stack Start      |  <-- Address of stack start for Thread -
   |       Stack Page 1     |               sizeof(pthread_thread_info)
   +------------------------+
   |       Stack Page 2     |
   +------------------------+
   |       Stack Page 3     |
   +------------------------+
   |            .           |
   |            .           |
   |            .           |
   +------------------------+  <-- StackRegionLastVPN()
   |       Stack Page N     |  <-- Stack grows downward
   +------------------------+
   |       Guard Page 2     |
   +------------------------+  <-- stackStartVPN() for Thread 1
   |       Stack Page 1     |
   +------------------------+
   |            .           |
   |            .           |
   |            .           |
   +------------------------+

    where N = MAX_THREAD_STACKS
*/
StackAllocator::STACK_REGION
StackAllocator::getStackRegion(size_t address, UserThread *thread) {
  size_t addr_vpn = addressToVPN(address);

  debug(THREADS_STACK_MANAGER,
        "Getting stack region for address %18zx, VPN: %zu\n", address,
        addr_vpn);

  size_t thread_id = thread->getTID();

  // check if it is exec param pointer page
  if (addr_vpn == exec_param_pointer_page_vpn_)
    return EXEC_POINTER_PAGE;

  // check if it is exec param page
  if (addr_vpn == exec_param_page_vpn_)
    return EXEC_PARAM_PAGE;

  size_t stack_region_low_vpn =
      getStackRegionStartVPN(MAX_THREADS_PER_PROCESS - 1);
  size_t stack_region_span = MAX_THREAD_STACKS + GUARD_PAGE - 1;
  if (stack_region_low_vpn > stack_region_span)
    stack_region_low_vpn -= stack_region_span;
  else
    stack_region_low_vpn = 0;

  if (addr_vpn < stack_region_low_vpn || addr_vpn > first_vpn_)
    return OUT_OF_STACK_REGION;

  // check if it is in the threads guard page
  if (addr_vpn == getStackRegionStartVPN(thread_id))
    return STACK_GUARD_PAGE;

  // check if it is in its own stack
  size_t stack_start_vpn = getStackStartVPN(thread_id);
  size_t stack_low_vpn = stack_start_vpn - (MAX_THREAD_STACKS - 1);
  if (addr_vpn >= stack_low_vpn && addr_vpn <= stack_start_vpn)
    return OWN_THREAD_STACK;

  // check if it is in another thread's stack
  size_t accessed_thread_id = vpnToThreadID(addr_vpn);
  if (accessed_thread_id >= MAX_THREADS_PER_PROCESS)
    return OUT_OF_STACK_REGION;
  size_t accessed_stack_start_vpn = getStackStartVPN(accessed_thread_id);

  // check if its in another thread's stack regions
  if (addr_vpn == getStackRegionStartVPN(accessed_thread_id))
    return STACK_GUARD_PAGE;

  size_t accessed_stack_low_vpn =
      accessed_stack_start_vpn - (MAX_THREAD_STACKS - 1);
  if (addr_vpn >= accessed_stack_low_vpn &&
      addr_vpn <= accessed_stack_start_vpn && accessed_thread_id != thread_id)
    return OTHER_THREAD_STACK;

  assert(false && "Something is not defined correctly in getStackRegion");
  return NOT_DEFINED;
}

void StackAllocator::setupThreadMemoryRegion(UserThread *thread) {

  debug(THREADS_STACK_MANAGER,
        "Setting up thread memory region for thread %zu\n", thread->getTID());

  size_t thread_id = thread->getTID();

  size_t guard_page_vpn_start = getStackRegionStartVPN(thread_id);
  debug(THREADS_STACK_MANAGER,
        "Guard pages for thread %zu start at vpn %zu and remain unmapped\n",
        thread_id, guard_page_vpn_start);
}

void *StackAllocator::setupThreadStack(UserThread *thread) {
  debug(THREADS_STACK_MANAGER, "Setting up thread stack for thread %zu\n",
        thread->getTID());

  size_t thread_id = thread->getTID();

  setupThreadMemoryRegion(thread);

  size_t stack_start_vpn = getStackStartVPN(thread_id);

  debug(THREADS_STACK_MANAGER,
        "Thread stack starts at vpn %zu and address %18zx\n and ends at vpn "
        "%zu\n",
        stack_start_vpn, vpnToPageStartAddress(stack_start_vpn),
        getNextGuardPageVPN(thread_id));

  size_t stack_top = vpnToPageStartAddress(stack_start_vpn);
  size_t info_bytes = sizeof(pthread_thread_info);
  size_t tls_data_bytes = 0;
  void *tls_data_address = nullptr;

  debug(THREADS_STACK_MANAGER,
        "Reserving %zu bytes for pthread_thread_info in thread %zu stack\n",
        info_bytes, thread->getTID());

  if (thread->process_->loader_->tls_info_.found) {
    const auto &tls = thread->process_->loader_->tls_info_;

    size_t tls_align = tls.phdr.p_align;
    tls_data_bytes = alignUp(tls.phdr.p_memsz, tls_align);

    debug(THREADS_STACK_MANAGER,
          "Reserving %zu bytes for TLS data in thread %zu stack\n",
          tls_data_bytes, thread->getTID());
  }

  void *info_start_address = (void *)(stack_top - info_bytes);
  if (tls_data_bytes)
    tls_data_address = (void *)((size_t)info_start_address - tls_data_bytes);

  // Keep stack pointer 16-byte aligned
  size_t sp = (stack_top - tls_data_bytes - info_bytes) & ~0xF;
  void *stack_start_address = (void *)sp;

  debug(THREADS_STACK_MANAGER,
        "Thread %zu stack setup complete. Stack start address: %18zx, "
        "pthread_thread_info address: %18zx, TLS data address: %18zx\n",
        thread->getTID(), (size_t)stack_start_address,
        (size_t)info_start_address, (size_t)tls_data_address);

  // create stack_info
  debug(THREADS_STACK_MANAGER, "Creating stack info for thread %zu\n",
        thread_id);
  stack_info_lock_.acquire();
  StackDescriptor *stack_info = getStackInfoForThread(thread_id);

  if (stack_info != nullptr) {
    assert(false &&
           "Stack info for thread already exists during stack setup - this "
           "should never happen");
  }

  stack_info = new StackDescriptor();
  stack_info->allocated = true;
  stack_info->stack_start = stack_start_address;
  stack_info->pthread_info_start = info_start_address;
  stack_info->tls_data_start = tls_data_address;
  stack_info->tls_data_size = tls_data_bytes;
  stack_info->tls_allocation_size = tls_data_bytes;

  setStackInfoForThread(thread_id, stack_info);
  stack_info_lock_.release();

  return stack_start_address;
}

bool StackAllocator::allocateStackForThread(size_t address) {

  size_t page_for_stack = FrameAlloc::instance()->allocPPN();

  stack_info_lock_.acquire();
  size_t thread_id = vpnToThreadID(addressToVPN(address));

  debug(THREADS_STACK_MANAGER, "Allocating stack for thread %zu\n", thread_id);

  size_t stack_start_vpn = getStackStartVPN(thread_id);
  UserThread *thread = process_->getThreadById(thread_id);
  size_t vpn_to_map = addressToVPN(address);
  size_t stack_low_vpn = stack_start_vpn - (MAX_THREAD_STACKS - 1);

  assert((vpn_to_map <= stack_start_vpn && vpn_to_map >= stack_low_vpn) &&
         "Address to allocate is outside of the stack region - this should "
         "never happen");

  // check if stack info already exists
  StackDescriptor *stack_info = getStackInfoForThread(thread_id);

  if (stack_info == nullptr) {
    debug(THREADS_STACK_MANAGER,
          "No stack info found for thread %zu, thread is not created\n",
          thread_id);
    stack_info_lock_.release();
    FrameAlloc::instance()->freePPN(page_for_stack);
    return false;
  }

  // check if the vpn is already mapped
  for (size_t vpn : stack_info->allocated_vpns) {
    if (vpn == vpn_to_map) {
      debug(THREADS_STACK_MANAGER,
            "Stack page at vpn %zu is already allocated for thread %zu\n",
            vpn_to_map, thread_id);
      stack_info_lock_.release();
      FrameAlloc::instance()->freePPN(page_for_stack);
      return true;
    }
  }

  if (stack_info->allocated_vpns.size() == MAX_THREAD_STACKS) {
    debug(THREADS_STACK_MANAGER,
          "Stack for thread %zu is already fully allocated (%zu pages)\n",
          thread_id, stack_info->allocated_vpns.size());
    stack_info_lock_.release();
    FrameAlloc::instance()->freePPN(page_for_stack);
    return true;
  }

  if (stack_info->allocated_vpns.size() > MAX_THREAD_STACKS) {
    assert(false && "StackDescriptor shows more allocated pages than "
                    "MAX_THREAD_STACKS - this should never happen");
  }

  debug(THREADS_STACK_MANAGER,
        "Allocating stack page at vpn %zu, ppn %zu and address %18zx\n",
        vpn_to_map, page_for_stack, vpnToPageStartAddress(vpn_to_map));

  bool vpn_mapped = thread->loader_->arch_memory_.mapPage(
      vpn_to_map, page_for_stack, 1, 1, false);

  assert(vpn_mapped && "Virtual page for stack was already mapped - this "
                       "should never happen");

  stack_info->allocated_vpns.push_back(vpn_to_map);

  // if its the first page also write the pthread_thread_info to it
  // and TLS if needed
  if (vpn_to_map == stack_start_vpn) {
    // PTHREAD THREAD INFO SETUP
    debug(THREADS_STACK_MANAGER,
          "Writing pthread_thread_info for thread %zu to stack page ppn "
          "%zu\n",
          thread->getTID(), page_for_stack);

    pthread_thread_info *thread_info =
        (pthread_thread_info *)(ArchMemory::getIdentAddressOfPPN(
                                    page_for_stack) +
                                PAGE_SIZE - sizeof(pthread_thread_info));

    thread_info->thread_id = thread->getTID();
    thread_info->block_schedule = 0;
    thread_info->waiting_list_blocked = 0;
    thread_info->next = nullptr;
    thread_info->waiting_on_mutex = nullptr;
    thread_info->waiting_on_semaphore = nullptr;
    // write init string
    for (size_t j = 0; j < sizeof(thread_info->init_string); j++) {
      thread_info->init_string[j] = INIT_STRING[j];
    }

    thread->setStackInfo(thread_info);

    debug(THREADS_STACK_MANAGER,
          "pthread_thread_info address %lx | tid: %zu \n", (size_t)thread_info,
          thread_info->thread_id);
    assert(thread->getTID() == thread_info->thread_id &&
           "Thread ID mismatch in pthread_thread_info");

    // TLS SETUP
    thread_info->tls_self = stack_info->pthread_info_start;
    thread_info->tls_data_base = stack_info->tls_data_start;
    thread_info->tls_data_size = stack_info->tls_data_size;

    if (thread->process_->loader_->tls_info_.found &&
        stack_info->tls_data_start) {
      const auto &tls = thread->process_->loader_->tls_info_;
      debug(THREADS_STACK_MANAGER,
            "Setting up TLS for thread %zu in stack page ppn %zu\n",
            thread->getTID(), page_for_stack);
      uint8_t *master_tls_address =
          (uint8_t *)thread->process_->loader_->tls_info_.ident_address;
      uint8_t *thread_tls_address = (uint8_t *)stack_info->tls_data_start;

      memcpy(thread_tls_address, master_tls_address, tls.phdr.p_memsz);

      // debug the TLS on Stack as size_t
      for (size_t i = 0; i < aostl::min(stack_info->tls_data_size, (size_t)64);
           i += sizeof(size_t)) {
        size_t value = *((size_t *)thread_tls_address + (i / sizeof(size_t)));
        debug(THREADS_STACK_MANAGER,
              "TLS for thread %zu at offset %zu: value %18zx\n",
              thread->getTID(), i, value);
      }

      debug(THREADS_STACK_MANAGER,
            "TLS for thread %zu set up at data address %18zx (size %zu bytes), "
            "pthread_thread_info at %18zx\n",
            thread->getTID(), (size_t)thread_tls_address,
            stack_info->tls_data_size, (size_t)thread_info);
    }
  }

  debug(THREADS_STACK_MANAGER,
        "Stack allocation done for thread %zu, total stack size is %zu "
        "pages\n",
        thread_id, stack_info->allocated_vpns.size());

  stack_info->allocated = true;
  setStackInfoForThread(thread->getTID(), stack_info);
  stack_info_lock_.release();
  return true;
}

const char *StackAllocator::getStackRegionString(STACK_REGION region) {
  switch (region) {
  case STACK_GUARD_PAGE:
    return "STACK_GUARD_PAGE";
  case EXEC_POINTER_PAGE:
    return "EXEC_POINTER_PAGE";
  case EXEC_PARAM_PAGE:
    return "EXEC_PARAM_PAGE";
  case OWN_THREAD_STACK:
    return "OWN_THREAD_STACK";
  case OTHER_THREAD_STACK:
    return "OTHER_THREAD_STACK";
  case OUT_OF_STACK_REGION:
    return "OUT_OF_STACK_REGION";
  case NOT_DEFINED:
    return "NOT_DEFINED";
  default:
    return "UNKNOWN_REGION";
  }
}

StackDescriptor *StackAllocator::getStackInfoForThread(size_t thread_id) {
  debug(THREADS_STACK_MANAGER, "Getting stack info for thread %zu\n",
        thread_id);

  assert(stack_info_lock_.isHeldBy(currentThread) &&
         "stack_info_lock_ must be held by currentThread");

  StackDescriptor *info = nullptr;
  auto it = stack_infos_.find(thread_id);
  if (it != stack_infos_.end())
    info = it->second;
  return info;
}

void StackAllocator::setStackInfoForThread(size_t thread_id,
                                               StackDescriptor *info) {
  assert(stack_info_lock_.isHeldBy(currentThread) &&
         "stack_info_lock_ must be held by currentThread");
  debug(THREADS_STACK_MANAGER, "Setting stack info for thread %zu\n",
        thread_id);
  stack_infos_[thread_id] = info;
}

StackDescriptor *StackAllocator::cloneStackInfoFrom(StackAllocator *other,
                                                  size_t thread_id) {
  debug(THREADS_STACK_MANAGER,
        "Cloning stack info for thread %zu from other StackAllocator\n",
        thread_id);

  if (!other)
    return nullptr;

  other->stack_info_lock_.acquire();
  StackDescriptor *copy = nullptr;
  auto it = other->stack_infos_.find(thread_id);
  if (it != other->stack_infos_.end() && it->second)
    copy = new StackDescriptor(*it->second);
  other->stack_info_lock_.release();
  return copy;
}

void StackAllocator::unmapStackForThread(UserThread *thread) {
  debug(THREADS_STACK_MANAGER, "Unmapping stack for thread %zu\n",
        thread->getTID());

  stack_info_lock_.acquire();
  StackDescriptor *info = getStackInfoForThread(thread->getTID());

  if (info == nullptr) {
    debug(THREADS_STACK_MANAGER,
          "No stack info found for thread %zu - probably because of EXEC\n",
          thread->getTID());
    stack_info_lock_.release();
    return;
  }

  assert(info->allocated && "StackDescriptor found but stack was not allocated - "
                            "this should never happen");

  debug(THREADS_STACK_MANAGER,
        "Unmapping stack starting at %18zx for thread %zu\n",
        (size_t)info->stack_start, thread->getTID());

  debug(THREADS_STACK_MANAGER, "Clearing stack info pointer in thread %zu\n",
        thread->getTID());
  thread->setStackInfo(nullptr);

  // unmap all stack pages
  debug(THREADS_STACK_MANAGER, "Unmapping %zu stack pages for thread %zu\n",
        info->allocated_vpns.size(), thread->getTID());
  for (size_t vpn : info->allocated_vpns) {
    thread->loader_->arch_memory_.unmapPage(vpn);
  }

  info->allocated_vpns.clear();

  delete info;
  stack_infos_.erase(thread->getTID());

  stack_info_lock_.release();
  debug(THREADS_STACK_MANAGER, "Unmapping stack done for thread %zu\n",
        thread->getTID());
}

void StackAllocator::resetThreadStacks() {
  // debug(THREADS_STACK_MANAGER, "Resetting all thread stacks\n");

  // unmapping of the pages is done in Loader destructor
  stack_info_lock_.acquire();
  for (auto it = stack_infos_.begin(); it != stack_infos_.end(); ++it) {
    StackDescriptor *info = it->second;
    delete info;
  }
  stack_infos_.clear();
  stack_info_lock_.release();
}

bool StackAllocator::writeExecParamPage(char *const argv[]) {
  debug(THREADS_STACK_MANAGER, "Writing exec param page\n");

  if ((size_t)argv < PAGE_SIZE) {
    debug(THREADS_STACK_MANAGER, "Error: argv is null - writing not needed \n");
    return true;
  }

  exec_param_page_ppn_ = FrameAlloc::instance()->allocPPN();
  exec_param_page_allocated_ = true;

  exec_param_pointer_page_ppn_ = FrameAlloc::instance()->allocPPN();
  exec_param_pointer_page_allocated_ = true;

  char *page_address =
      (char *)ArchMemory::getIdentAddressOfPPN(exec_param_page_ppn_);

  char **pointer_page_address =
      (char **)ArchMemory::getIdentAddressOfPPN(exec_param_pointer_page_ppn_);

  char *page_address_user =
      (char *)(vpnToPageStartAddress(exec_param_page_vpn_) - PAGE_SIZE);

  debug(THREADS_STACK_MANAGER, "Starting to write exec param page\n");
  // write argv to the page
  size_t offset = 0;
  size_t argv_count = 0;
  size_t totat_bytes_written = 0;
  for (size_t i = 0; argv[i] != nullptr; i++) {

    if ((argv_count * sizeof(char *)) > PAGE_SIZE) {
      debug(THREADS_STACK_MANAGER, "Error: Exec param page overflow while "
                                   "writing argv pointers: seems to "
                                   "be to big\n");

      FrameAlloc::instance()->freePPN(exec_param_page_ppn_);
      FrameAlloc::instance()->freePPN(exec_param_pointer_page_ppn_);

      exec_param_page_allocated_ = false;
      exec_param_pointer_page_allocated_ = false;

      return false;
    }

    size_t arg_length = strlen(argv[i]) + 1; // +1 for null terminator
    totat_bytes_written += arg_length;

    if (totat_bytes_written > PAGE_SIZE) {
      debug(THREADS_STACK_MANAGER,
            "Error: Exec param page overflow while writing argv[%zu]: "
            "seems to "
            "be to big | length %ld \n",
            i, arg_length);

      FrameAlloc::instance()->freePPN(exec_param_page_ppn_);
      FrameAlloc::instance()->freePPN(exec_param_pointer_page_ppn_);

      exec_param_page_allocated_ = false;
      exec_param_pointer_page_allocated_ = false;

      return false;
    }

    // write one argument at a time
    for (size_t j = 0; j < arg_length; j++) {
      page_address[offset + j] = argv[i][j];
    }

    // write pointer to the argument in the pointer page
    pointer_page_address[argv_count] =
        (char *)((size_t)page_address_user + offset);

    offset += arg_length;

    debug(THREADS_STACK_MANAGER,
          "Wrote argv[%zu]: %s to exec param page at offset %zu\n", i, argv[i],
          offset - arg_length);

    argv_count++;
  }

  // null terminate the argv array in pointer page
  pointer_page_address[argv_count] = nullptr;

  argv_count_ = argv_count;

  debug(THREADS_STACK_MANAGER, "Exec param page writing done\n");
  return true;
}

size_t StackAllocator::getArgvCount() {
  debug(THREADS_STACK_MANAGER, "Getting argv count: %zu\n", argv_count_);
  return argv_count_;
}

char **StackAllocator::readExecParamPage() {
  debug(THREADS_STACK_MANAGER, "Reading exec param page\n");

  if (!exec_param_pointer_page_allocated_) {
    debug(THREADS_STACK_MANAGER,
          "Error: Exec param pointer page not allocated - reading not "
          "possible \n");
    return nullptr;
  }

  char **page_address =
      (char **)(vpnToPageStartAddress(exec_param_pointer_page_vpn_) -
                PAGE_SIZE);

  debug(THREADS_STACK_MANAGER, "Exec param page address: %18zx\n",
        (size_t)page_address);

  // return userspace pointer to argv
  return (char **)page_address;
}

void StackAllocator::mapParamPages(Loader *loader) {
  debug(THREADS_STACK_MANAGER, "Mapping exec param pages in new loader\n");

  assert(loader != nullptr && "Loader pointer is null");

  if (exec_param_page_allocated_) {
    debug(THREADS_STACK_MANAGER,
          "Mapping exec param page at vpn %zu and address %18zx\n",
          exec_param_page_vpn_, vpnToPageStartAddress(exec_param_page_vpn_));

    bool mapped = loader->arch_memory_.mapPage(
        exec_param_page_vpn_, exec_param_page_ppn_, 1, 1, false);

    assert(mapped &&
           "Virtual page for exec param page was already mapped - this should "
           "never happen");
  }

  if (exec_param_pointer_page_allocated_) {
    debug(THREADS_STACK_MANAGER,
          "Mapping exec param pointer page at vpn %zu and address %18zx\n",
          exec_param_pointer_page_vpn_,
          vpnToPageStartAddress(exec_param_pointer_page_vpn_));

    bool mapped =
        loader->arch_memory_.mapPage(exec_param_pointer_page_vpn_,
                                     exec_param_pointer_page_ppn_, 1, 1, false);

    assert(mapped &&
           "Virtual page for exec param pointer page was already mapped - "
           "this should never happen");
  }
}
