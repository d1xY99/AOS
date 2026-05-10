#include "FaultDispatch.h"
#include "ArchInterrupts.h"
#include "ArchMemory.h"
#include "ArchThreads.h"
#include "HeapIface.h"
#include "InvPageTable.h"
#include "Loader.h"
#include "FrameAlloc.h"
#include "Scheduler.h"
#include "ShMemRegistry.h"
#include "SwapEngine.h"
#include "Syscall.h"
#include "Thread.h"
#include "StackAllocator.h"
#include "UserProcess.h"
#include "UserThread.h"
#include "FsOps.h"
#include "assert.h"
#include "debug.h"
#include "kprintf.h"
#include "kstring.h"
#include "offsets.h"
#include "paging-definitions.h"
#include "kernel_user_shared.h"
#include "syscall-definitions.h"
extern "C" void arch_contextSwitch();

const size_t FaultDispatch::null_reference_check_border_ = PAGE_SIZE;

bool handle_copy_on_write_fault(size_t address, size_t vpn, PageTableEntry &pte,
                                uint64 new_ppn, UserThread *user_thread,
                                ArchMemory &arch_memory, uint64 *old_ppn_out) {
  if (!currentThread || currentThread->type_ != Thread::USER_THREAD) {
    if (new_ppn)
      FrameAlloc::instance()->freePPN(new_ppn);
    return false;
  }

  if (!user_thread || !user_thread->process_ ||
      !user_thread->process_->loader_) {
    if (new_ppn)
      FrameAlloc::instance()->freePPN(new_ppn);
    return false;
  }

  StackAllocator::STACK_REGION region =
      user_thread->process_->thread_stack_manager_->getStackRegion(address,
                                                                   user_thread);
  if (region == StackAllocator::STACK_GUARD_PAGE) {
    debug(PAGEFAULT, "You are accessing a guard page of your stack.\n");
    if (new_ppn)
      FrameAlloc::instance()->freePPN(new_ppn);
    return false;
  }

  // check if the page is copy-on-write mapped and present
  if (!pte.present || !pte.copy_on_write) {
    if (new_ppn)
      FrameAlloc::instance()->freePPN(new_ppn);
    return false;
  }

  // if already writable, nothing to do
  if (pte.writeable) {
    if (new_ppn)
      FrameAlloc::instance()->freePPN(new_ppn);
    if (old_ppn_out)
      *old_ppn_out = 0;
    return true;
  }

  assert(new_ppn && "COW handling requires a fresh physical page");

  // allocate a new physical page and copy the contents
  PageTableEntry old_pte = pte;
  uint64 old_ppn = pte.page_ppn;
  bool was_present = pte.present;
  if (was_present)
    pte.present = 0;

  // duplicate the shared page so this process keeps its private state
  memcpy((void *)ArchMemory::getIdentAddressOfPPN(new_ppn),
         (void *)ArchMemory::getIdentAddressOfPPN(old_ppn), PAGE_SIZE);

  // Only write the thread info if its the first page of the stack
  if (user_thread->process_->thread_stack_manager_->getStackStartVPN(
          user_thread->getTID()) == vpn) {
    debug(PAGEFAULT, "Setting up thread info structure for COW page fault\n");
    // set up the thread info structure at the end of the new stack page

    pthread_thread_info *new_thread_info =
        (pthread_thread_info *)(ArchMemory::getIdentAddressOfPPN(new_ppn) +
                                PAGE_SIZE - sizeof(pthread_thread_info));

    new_thread_info->thread_id = user_thread->getTID();
    new_thread_info->waiting_list_blocked = 0;
    new_thread_info->block_schedule = 0;
    new_thread_info->waiting_on_mutex = nullptr;
    new_thread_info->waiting_on_semaphore = nullptr;
    new_thread_info->next = nullptr;

    // write init string
    for (size_t j = 0; j < sizeof(new_thread_info->init_string); j++) {
      new_thread_info->init_string[j] = INIT_STRING[j];
    }

    user_thread->setStackInfo(new_thread_info);
  }

  pte.page_ppn = new_ppn;
  pte.writeable = 1;
  pte.copy_on_write = 0;
  pte.shared = 0;

  if (was_present)
    pte.present = 1;

  InvPageTable::instance()->lock_.acquire();
  InvPageTable::instance()->findAndRemoveMappingFromIPTEntry(
      &old_pte, &arch_memory, vpn);
  InvPageTable::instance()->insertMapping(
      new_ppn, user_thread->process_, vpn);
  InvPageTable::instance()->lock_.release();

  if (old_ppn_out)
    *old_ppn_out = old_ppn;

  return true;
}

inline bool FaultDispatch::checkPageFaultIsValid(size_t address, bool user,
                                                    bool present,
                                                    bool switch_to_us,
                                                    bool cow_fault) {
  assert((user == switch_to_us) &&
         "Thread is in user mode even though is should not be.");
  assert(!(address < USER_BREAK && currentThread->loader_ == 0) &&
         "Thread accesses the user space, but has no loader.");
  assert(!(user && currentThread->user_registers_ == 0) &&
         "Thread is in user mode, but has no valid registers.");

  if (currentThread && currentThread->type_ == Thread::USER_THREAD) {
    UserThread *user_thread = (UserThread *)currentThread;

    // HEAP CHECK
    HeapIface::HEAP_REGION heap_region =
        user_thread->process_->heap_manager_->getHeapRegion(address);
    if (heap_region == HeapIface::IN_HEAP) {
      debug(PAGEFAULT, "You are accessing the heap region of your process.\n");
      goto check;
    }

    // STACK CHECK
    StackAllocator::STACK_REGION region =
        user_thread->process_->thread_stack_manager_->getStackRegion(
            address, user_thread);
    if (region == StackAllocator::STACK_GUARD_PAGE) {
      debug(PAGEFAULT, "You are accessing a guard page of your stack.\n");
      return false;
    }
  }

check:

  if (address < null_reference_check_border_) {
    debug(PAGEFAULT, "Maybe you are dereferencing a null-pointer.\n");
  } else if (!user && address >= USER_BREAK) {
    debug(PAGEFAULT, "You are accessing an invalid kernel address.\n");
  } else if (user && address >= USER_BREAK) {
    debug(PAGEFAULT, "You are accessing a kernel address in user-mode.\n");
  } else if (present && !cow_fault) {
    debug(PAGEFAULT,
          "You got a pagefault even though the address is mapped.\n");
  } else {
    // everything seems to be okay
    return true;
  }
  return false;
}

inline void FaultDispatch::handlePageFault(size_t address, bool user,
                                              bool present, bool writing,
                                              bool fetch, bool switch_to_us) {

  if (PAGEFAULT & OUTPUT_ENABLED)
    kprintfd("\n");
  debug(PAGEFAULT, "Address: %18zx - Thread %zu: %s (%p)\n", address,
        currentThread->getTID(), currentThread->getName(), currentThread);
  debug(PAGEFAULT,
        "Flags: %spresent, %s-mode, %s, %s-fetch, switch to userspace: %1d\n",
        present ? "    " : "not ", user ? "  user" : "kernel",
        writing ? "writing" : "reading", fetch ? "instruction" : "    operand",
        switch_to_us);

  ArchThreads::printThreadRegisters(currentThread, false);

  size_t vpn = address / PAGE_SIZE;
  UserThread *user_thread = nullptr;
  if (currentThread && currentThread->type_ == Thread::USER_THREAD)
    user_thread = static_cast<UserThread *>(currentThread);

  uint64 new_ppn = 0;
  uint64 old_ppn = 0;
  if (present && writing)
    new_ppn = FrameAlloc::instance()->allocPPN();

  ArchMemory &arch_memory = currentThread->loader_->arch_memory_;
  arch_memory.arch_memory_lock_.acquire();
  ArchMemoryMapping m = arch_memory.resolveMapping(vpn);

  PageTableEntry *pte = m.pt ? &m.pt[m.pti] : nullptr;
  bool cow_fault = pte && pte->present && pte->copy_on_write && writing;
  bool swapped_out = pte && !pte->present && pte->swapped_out;

  // 1) Check whether this fault is even allowed
  if (!checkPageFaultIsValid(address, user, present, switch_to_us, cow_fault)) {
    arch_memory.arch_memory_lock_.release();
    if (new_ppn)
      FrameAlloc::instance()->freePPN(new_ppn);
    goto invalid_fault;
  }

  // 2) Swapped-out page
  if (swapped_out) {
    size_t old_swap_slot = pte->page_ppn;
    arch_memory.arch_memory_lock_.release();
    if (!new_ppn) {
      new_ppn = FrameAlloc::instance()->allocPPN();
    }
    debug(PAGEFAULT, "FaultDispatch: Page is swapped out, swapping in\n");
    SwapEngine::instance()->handlePageIsSwappedOut(old_swap_slot, new_ppn);
    debug(PAGEFAULT, "FaultDispatch: Swapped in page successfully\n");
    goto end_page_fault;
  }

  // 3) Copy-on-write faults
  if (cow_fault) {
    bool cow_handled = handle_copy_on_write_fault(
        address, vpn, *pte, new_ppn, user_thread, arch_memory, &old_ppn);
    new_ppn = 0;
    arch_memory.arch_memory_lock_.release();
    if (cow_handled) {
      debug(PAGEFAULT, "Handled copy-on-write page fault at address %18zx.\n",
            address);
      ArchMemory::flushTlb();
      if (old_ppn)
        FrameAlloc::instance()->freePPN(old_ppn);
      goto end_page_fault;
    }
    goto invalid_fault;
  }
  arch_memory.arch_memory_lock_.release();
  if (new_ppn)
    FrameAlloc::instance()->freePPN(new_ppn);

  // 4) Shared memory mappings
  if (user_thread) {
    UserProcess *process = user_thread->process_;
    if (process && process->sharedMemManager() &&
        address >= MIN_SHARED_MEM_ADDRESS && address < MAX_SHARED_MEM_ADDRESS) {
      ShMemRegistry *manager = process->sharedMemManager();
      if (manager) {
        bool handled = manager->handleSharedPageFault(address, writing);
        if (handled)
          goto end_page_fault;
        goto invalid_fault;
      }
    }
  }

  // 5) Demand-mapping for user threads (heap / stack)
  if (user_thread) {

    // --- HEAP ---
    HeapIface::HEAP_REGION heap_region =
        user_thread->process_->heap_manager_->getHeapRegion(address);

    if (heap_region == HeapIface::IN_HEAP) {
      debug(PAGEFAULT, "Accessing heap region, trying to map page on demand\n");

      if (user_thread->process_->heap_manager_->allocateHeapPage(address)) {
        debug(PAGEFAULT, "Successfully allocated heap page on demand\n");
        goto end_page_fault;
      }
    }

    // --- STACK ---
    StackAllocator::STACK_REGION stack_region =
        user_thread->process_->thread_stack_manager_->getStackRegion(
            address, user_thread);

    // Threads may also map stacks of other threads
    if (stack_region == StackAllocator::OWN_THREAD_STACK ||
        stack_region == StackAllocator::OTHER_THREAD_STACK) {

      debug(PAGEFAULT,
            "Accessing thread stack, but stack is not allocated for thread\n");

      if (user_thread->process_->thread_stack_manager_->allocateStackForThread(
              address)) {
        debug(PAGEFAULT, "Successfully allocated stack for thread\n");
        goto end_page_fault;
      }

      debug(PAGEFAULT, "Failed to allocate stack for thread\n");
      goto invalid_fault;
    } else {
      debug(PAGEFAULT, "Not accessing thread stack region\n");
    }
  }

  // 4) Valid fault, but not handled by heap/stack/cow: try loading from pager
  debug(PAGEFAULT, "Page fault seems to be valid, trying to load page\n");
  currentThread->loader_->loadPage(address);
  goto end_page_fault;

invalid_fault:
  // the page-fault seems to be faulty, print out the thread stack traces
  ArchThreads::printThreadRegisters(currentThread, true);
  currentThread->printBacktrace(true);

  if (currentThread->loader_) {
    // writePageFaultFailedMessage(address, user, present, writing, fetch);
    Syscall::exit(9999);
  } else {
    currentThread->kill();
  }
  return;

end_page_fault:
  debug(PAGEFAULT, "Page fault handling finished for Address: %18zx.\n",
        address);
}

void FaultDispatch::enterPageFault(size_t address, bool user, bool present,
                                      bool writing, bool fetch) {
  assert(currentThread && "You have a pagefault, but no current thread");
  // save previous state on stack of currentThread
  uint32 saved_switch_to_userspace = currentThread->switch_to_userspace_;

  currentThread->switch_to_userspace_ = 0;
  currentThreadRegisters = currentThread->kernel_registers_;
  ArchInterrupts::enableInterrupts();

  handlePageFault(address, user, present, writing, fetch,
                  saved_switch_to_userspace);

  ArchInterrupts::disableInterrupts();
  currentThread->switch_to_userspace_ = saved_switch_to_userspace;
  if (currentThread->switch_to_userspace_)
    currentThreadRegisters = currentThread->user_registers_;
}

void FaultDispatch::writePageFaultFailedMessage(size_t address, bool user,
                                                   bool present, bool writing,
                                                   bool fetch) {

  if (!(currentThread && currentThread->type_ == Thread::USER_THREAD)) {
    debug(PAGEFAULT,
          "No user thread available, skipping crashed programm message\n");
    return;
  }

  debug(PAGEFAULT, "Programm crashed message print\n");

  const char *stack_region = "N/A";

  // get the accessed region
  UserThread *user_thread = (UserThread *)currentThread;

  if (!user_thread->process_ || !user_thread->process_->thread_stack_manager_) {
    debug(PAGEFAULT,
          "No process or thread stack manager available, skipping crashed "
          "programm message\n");
    return;
  }

  StackAllocator::STACK_REGION region =
      user_thread->process_->thread_stack_manager_->getStackRegion(address,
                                                                   user_thread);
  stack_region =
      user_thread->process_->thread_stack_manager_->getStackRegionString(
          region);

  aostl::string FAIL_MESSAGE = "\n--- PROGRAMM CRASHED ---\n";

  // Access type
  FAIL_MESSAGE += "Access type: ";
  FAIL_MESSAGE += writing ? "write" : "read";
  FAIL_MESSAGE += "\n";

  // Fetch type
  FAIL_MESSAGE += "Fetch type: ";
  FAIL_MESSAGE += fetch ? "instruction" : "operand";
  FAIL_MESSAGE += "\n";

  // Mode
  FAIL_MESSAGE += "Mode: ";
  FAIL_MESSAGE += user ? "user" : "kernel";
  FAIL_MESSAGE += "\n";

  // Present
  FAIL_MESSAGE += "Present: ";
  FAIL_MESSAGE += present ? "yes" : "no";
  FAIL_MESSAGE += "\n";

  // Accessed region
  FAIL_MESSAGE += "Accessed region: ";
  FAIL_MESSAGE += stack_region;
  FAIL_MESSAGE += "\n";

  FAIL_MESSAGE += "-----------------------\n\n";

  kprintf("%s", FAIL_MESSAGE.c_str());
}
