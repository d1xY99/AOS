#include "HeapIface.h"
#include "ArchMemory.h"
#include "ArchThreads.h"
#include "Loader.h"
#include "FrameAlloc.h"
#include "Scheduler.h"
#include "UserProcess.h"
#include "assert.h"
#include "debug.h"
#include "paging-definitions.h"
#include "kernel_user_shared.h"

HeapIface::HeapIface(UserProcess *process) : lock_("HeapIface::lock_") {

  process_ = process;
  first_vpn_ = HEAP_START / PAGE_SIZE;
  if (HEAP_ASLR) {
    size_t max_offset = MAX_HEAP_ASLR_OFFSET;
    if (max_offset > 0) {
      size_t random_range = max_offset + 1;
      size_t aslr_offset = Scheduler::instance()->getRandomNumber(random_range);
      first_vpn_ += aslr_offset;
      debug(HEAP_MANAGER,
            "Applying ASLR offset of %zu pages to heap for process %ld\n",
            aslr_offset, process_->getPID());
    }
  }
  heap_end_address_ = (void *)(first_vpn_ * PAGE_SIZE);
  heap_initialized_ = false;

  debug(HEAP_MANAGER, "HeapIface created\n");
}

HeapIface::HeapIface(HeapIface *other) : lock_("HeapIface::lock_") {
  debug(HEAP_MANAGER, "Copying HeapIface\n");

  process_ = nullptr;
  first_vpn_ = other->first_vpn_;
  heap_end_address_ = other->heap_end_address_;
  heap_initialized_ = other->heap_initialized_;

  // Copy allocated pages
  other->lock_.acquire();
  for (auto &entry : other->allocated_pages_) {
    size_t vpn = entry.first;
    HeapPageInfo *info = entry.second;

    HeapPageInfo *new_info = new HeapPageInfo();
    new_info->allocated = info->allocated;

    allocated_pages_[vpn] = new_info;
  }

  debug(HEAP_MANAGER,
        "Copied %zu allocated heap pages from other HeapIface\n",
        allocated_pages_.size());
  other->lock_.release();

  debug(HEAP_MANAGER, "HeapIface copied\n");
}

HeapIface::~HeapIface() {
  debug(HEAP_MANAGER, "Destroying HeapIface\n");
  resetHeapStacks();
  debug(HEAP_MANAGER, "HeapIface destroyed\n");
}

void HeapIface::resetHeapStacks() {
  debug(HEAP_MANAGER, "Resetting heap manager\n");
  lock_.acquire();
  // Unmapping of the pages is done in Loader destructor
  for (auto it = allocated_pages_.begin(); it != allocated_pages_.end(); ++it) {
    HeapPageInfo *info = it->second;
    delete info;
  }
  allocated_pages_.clear();
  lock_.release();
}

bool HeapIface::setHeapEnd(void *end_address) {
  debug(HEAP_MANAGER, "Setting heap end address to %18zx\n",
        (size_t)end_address);

  lock_.acquire();
  size_t end_vpn = (size_t)end_address / PAGE_SIZE;

  if (end_vpn > HIGHEST_USER_VPN) {
    debug(HEAP_MANAGER,
          "Error: Requested heap end vpn %zu is outside user address space "
          "(highest vpn: %zu)\n",
          end_vpn, (size_t)HIGHEST_USER_VPN);
    lock_.release();
    return false;
  }

  // Heap grows upwards
  if (end_vpn < first_vpn_) {
    debug(HEAP_MANAGER,
          "Error: Requested heap end vpn %zu is below heap region (first "
          "vpn: %zu)\n",
          end_vpn, first_vpn_);
    lock_.release();
    return false;
  }

  heap_end_address_ = end_address;

  // Unmap pages that are no longer needed
  for (auto it = allocated_pages_.begin(); it != allocated_pages_.end();) {
    size_t vpn = it->first;
    if (vpn > end_vpn) {
      debug(HEAP_MANAGER,
            "Unmapping heap page vpn %zu for process %s | PID %ld\n", vpn,
            process_->minixfs_filename_.c_str(), process_->getPID());

      bool ret = process_->loader_->arch_memory_.unmapPage(vpn);

      assert(ret && "Heap page vpn was not mapped - this should never happen");

      delete it->second;
      it = allocated_pages_.erase(it);
    } else {
      ++it;
    }
  }

  if (end_address > getHeapStartVirtualAddress())
    heap_initialized_ = true;
  else
    heap_initialized_ = false;

  lock_.release();
  return true;
}

void *HeapIface::incHeapBy(size_t size) {
  debug(HEAP_MANAGER,
        "Increasing heap by %zu bytes (current end address: %18zx)\n", size,
        (size_t)heap_end_address_);

  void *old_end = heap_end_address_;
  size_t new_end_address = (size_t)heap_end_address_ + size;
  int ret = setHeapEnd((void *)new_end_address);

  return ret ? old_end : (void *)-1;
}

void *HeapIface::getHeapEndVirtualAddress() { return heap_end_address_; }

void *HeapIface::getHeapStartVirtualAddress() {
  return (void *)(first_vpn_ * PAGE_SIZE);
}

HeapIface::HEAP_REGION HeapIface::getHeapRegion(size_t address) {
  size_t addr_vpn = address / PAGE_SIZE;

  if (!heap_initialized_) {
    return OUT_OF_HEAP;
  }

  if (address >= (size_t)heap_end_address_ || addr_vpn < first_vpn_) {
    return OUT_OF_HEAP;
  }

  return IN_HEAP;
}

aostl::string HeapIface::getHeapRegionPrintable(HEAP_REGION region) {
  switch (region) {
  case IN_HEAP:
    return "IN_HEAP";
  case OUT_OF_HEAP:
    return "OUT_OF_HEAP";
  default:
    return "UNKNOWN_REGION";
  }
}

bool HeapIface::allocateHeapPage(size_t address) {
  debug(HEAP_MANAGER,
        "Allocating heap page for address %18zx on demand (current heap end "
        "address: %18zx)\n",
        address, (size_t)heap_end_address_);

  size_t addr_vpn = address / PAGE_SIZE;
  size_t ppn = FrameAlloc::instance()->allocPPN();

  lock_.acquire();

  debug(HEAP_MANAGER,
        "Current allocated heap pages for process %s | PID %ld: %zu\n",
        process_->minixfs_filename_.c_str(), process_->getPID(),
        allocated_pages_.size());

  // find if the vpn is already allocated
  if (allocated_pages_.find(addr_vpn) != allocated_pages_.end()) {
    debug(HEAP_MANAGER,
          "Heap page vpn %zu is already allocated for process %s | PID %ld\n",
          addr_vpn, process_->minixfs_filename_.c_str(), process_->getPID());
    lock_.release();
    FrameAlloc::instance()->freePPN(ppn);
    return true;
  }

  HeapPageInfo *info = new HeapPageInfo();
  info->allocated = true;
  addAllocatedPage(addr_vpn, info);
  lock_.release();

  debug(HEAP_MANAGER,
        "Mapping heap page vpn %zu to ppn %zu for process %s | PID %ld\n",
        addr_vpn, ppn, process_->minixfs_filename_.c_str(), process_->getPID());

  bool mapped =
      process_->loader_->arch_memory_.mapPage(addr_vpn, ppn, 1, 1, false);

  assert(mapped &&
         "Heap page vpn was already mapped - this should never happen");

  return true;
}

void HeapIface::addAllocatedPage(size_t vpn, HeapPageInfo *info) {
  assert(lock_.isHeldBy(currentThread) &&
         "HeapIface::addAllocatedPage called without holding the lock");

  allocated_pages_[vpn] = info;
}
