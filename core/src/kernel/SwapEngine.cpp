#include "SwapEngine.h"
#include "ArchMemory.h"
#include "Bitmap.h"
#include "InvPageTable.h"
#include "FrameAlloc.h"
#include "Scheduler.h"
#include "ShMemRegistry.h"
#include "Thread.h"
#include "assert.h"
#include "debug.h"
#include "memblock.h"
#include "paging-definitions.h"
#include "kernel_user_shared.h"
#include "types.h"

SwapEngine *SwapEngine::instance_ = nullptr;

SwapEngine *SwapEngine::instance() {
  if (unlikely(!instance_))
    instance_ = new SwapEngine();
  return instance_;
}

SwapEngine::SwapEngine()
    : swap_request_lock_("SwapEngine::swap_request_lock_") {

  schedule_swapping_thread_ = false;
  pre_swapping_active_ = false;
  pra_ = PageReplacementAlgorithm::RANDOM;

  current_pre_swap_request_ = new SwapReq(SWAP_TYPE::PRE_SWAP_OUT);

  swap_statistic_ = {0, 0, 0, 0};
  swap_device_ = BlockDevTable::getInstance()->getDeviceByNumber(SWAP_DEVICE_NR);
  swap_device_->setBlockSize(PAGE_SIZE);
  debug(SWAP,
        "SwapEngine: Swap device set to device number %d, name: %s, max space "
        "%u\n",
        SWAP_DEVICE_NR, swap_device_->getName(), swap_device_->getNumBlocks());
  swap_device_bitmap_ = new Bitmap(swap_device_->getNumBlocks());
  swap_device_bitmap_->setBit(0);

  swap_slot_checksums_ = new uint32[swap_device_->getNumBlocks()];
  memset(swap_slot_checksums_, 0, swap_device_->getNumBlocks() * sizeof(uint32));

  debug(SWAP, "SwapEngine: Create swapped out pages entries array\n");
  swapped_out_pages_entries_ =
      new InvPageTableEntry *[swap_device_->getNumBlocks()];

  debug(SWAP, "Calculating swapable PPN range...\n");
  size_t total_physical_pages = FrameAlloc::instance()->getTotalNumPages();
  size_t allowed_pages_for_user = FrameAlloc::instance()->getNumPagesForUser();

  debug(SWAP, "Total physical pages: %zu, allowed pages for user: %zu\n",
        total_physical_pages, allowed_pages_for_user);

  ppn_swap_min_ = total_physical_pages - allowed_pages_for_user;
  ppn_swap_max_ = total_physical_pages - 1;

  // Init PRA data
  current_clock_pointer_ = ppn_swap_min_;

  debug(SWAP, "Swapable PPN range: [%u - %u]\n", ppn_swap_min_, ppn_swap_max_);
  debug(SWAP, "SwapEngine: Initialized\n");
}

SwapEngine::~SwapEngine() {

  delete current_pre_swap_request_;
  delete[] swap_slot_checksums_;

  debug(SWAP, "SwapEngine: Destroyed\n");
}

uint32 SwapEngine::handleThreadNeedsFreePage() {
  size_t thread_id = currentThread->getTID();
  debug(SWAP, "SwapEngine: Thread %zu needs memory\n", thread_id);
  uint32 found_ppn = 0;

  SwapReq *swap_request = new SwapReq(SWAP_TYPE::SWAP_OUT);

  // Add the swap request and wait until it's done
  swap_request_lock_.acquire();
  addSwapRequest(swap_request);
  swap_request_lock_.release();

  swap_request->info.wait_info.lock.acquire();
  while (!swap_request->done) {
    debug(SWAP, "SwapEngine: Thread %zu waiting for swap to complete\n",
          thread_id);
    swap_request->info.wait_info.condition.wait();
  }
  found_ppn = swap_request->info.dest_ppn;
  swap_request->info.wait_info.lock.release();

  assert(swap_request->done && "Swap not done but woke up!");
  assert(found_ppn != 0 && "No ppn found after swap!");

  debug(SWAP, "SwapEngine: Thread %zu swap completed, got PPN %u\n", thread_id,
        found_ppn);

  debug(SWAP, "SwapEngine: Cleaning up swap request\n");
  delete swap_request;

  return found_ppn;
}

void SwapEngine::printSwapStatistics() {
  debug(SWAP, "Swap Statistics:\n");
  debug(SWAP, "  Swap In Requests: %zu\n", swap_statistic_.swap_in_requests);
  debug(SWAP, "  Swap Out Requests: %zu\n", swap_statistic_.swap_out_requests);
  debug(SWAP, "  Pages Swapped In: %zu\n", swap_statistic_.pages_swapped_in);
  debug(SWAP, "  Pages Swapped Out: %zu\n", swap_statistic_.pages_swapped_out);
}

void SwapEngine::handlePageIsSwappedOut(size_t swap_slot, uint32 new_ppn) {
  debug(SWAP, "SwapEngine: Swapping in Page from disk\n");
  SwapReq *swap_request = nullptr;

  // Checking if a SWAP_IN request for this page already exists
  swap_request_lock_.acquire();

  // iterate through existing swap requests
  for (SwapReqNode *node = swap_requests_head_; node != nullptr;
       node = node->next) {
    SwapReq *req = node->req;
    if (req->type == SWAP_TYPE::SWAP_IN &&
        req->info.swap_slot_to_read == swap_slot) {
      swap_request = req;
      debug(SWAP,
            "SwapEngine: Found existing SWAP_IN request for requested page\n");
      // Free the newly allocated PPN since an existing request will handle it
      FrameAlloc::instance()->freePPN(new_ppn);
      break;
    }
  }

  if (!swap_request) {
    // No existing request; create and enqueue a new one
    swap_request = new SwapReq(SWAP_TYPE::SWAP_IN);
    swap_request->info.swap_slot_to_read = swap_slot;
    swap_request->info.evicted_ppn = new_ppn;

    InvPageTable::instance()->lock_.acquire();
    InvPageTable::instance()->deleteIPTEntry(new_ppn);
    InvPageTable::instance()->lock_.release();

    addSwapRequest(swap_request);
  }

  // Wait until the swap-in is done
  swap_request->info.wait_info.lock.acquire();
  swap_request->waiters++;
  swap_request_lock_.release();
  while (!swap_request->done) {
    debug(SWAP, "SwapEngine: Waiting for swap-in to complete\n");
    swap_request->info.wait_info.condition.wait();
  }

  assert(swap_request->done && "Swap-in not done but woke up!");

  // Last thread leaving cleans up the request
  swap_request->waiters--;
  bool delete_now = (swap_request->waiters == 0);
  swap_request->info.wait_info.lock.release();

  if (delete_now) {
    debug(SWAP, "SwapEngine: Swap-in done, cleaning up swap request\n");
    delete swap_request;
  }

  debug(SWAP, "SwapEngine: Swap-in completed\n");
}

void SwapEngine::addSwapRequest(SwapReq *request) {
  debug(SWAP, "SwapEngine: Adding swap request of type %s\n",
        swapTypeToString(request->type));
  assert(swap_request_lock_.isHeldBy(currentThread) &&
         "SwapEngine::addSwapRequest: swap_request_lock_ not held by current "
         "thread!");
  addSwapRequestNode(request);
  schedule_swapping_thread_ = true;
}

void SwapEngine::addSwapRequestNode(SwapReq *request) {
  SwapReqNode *node = new SwapReqNode(request);
  if (!swap_requests_tail_) {
    swap_requests_head_ = swap_requests_tail_ = node;
  } else {
    swap_requests_tail_->next = node;
    swap_requests_tail_ = node;
  }
}

SwapReq *SwapEngine::popSwapRequestFront() {
  if (!swap_requests_head_)
    return nullptr;
  SwapReqNode *node = swap_requests_head_;
  swap_requests_head_ = node->next;
  if (!swap_requests_head_)
    swap_requests_tail_ = nullptr;
  SwapReq *req = node->req;
  delete node;
  return req;
}

const char *SwapEngine::swapTypeToString(SWAP_TYPE type) {
  switch (type) {
  case SWAP_IN:
    return "SWAP_IN";
  case SWAP_OUT:
    return "SWAP_OUT";
  default:
    return "UNKNOWN_SWAP_TYPE";
  }
}

void SwapEngine::handleSwapRequests() {
  debug(SWAP, "SwapEngine: Handling swap requests\n");

  swap_request_lock_.acquire();

  SwapReq *request = nullptr;

  while ((request = popSwapRequestFront()) != nullptr) {

    debug(SWAP, "SwapEngine: Handling swap request of type %s\n",
          swapTypeToString(request->type));

    InvPageTable::instance()->lock_.acquire();
    FrameAlloc::instance()->lock_.acquire();
    switch (request->type) {
    case SWAP_IN:
      handleSwapInRequest(request);
      break;
    case SWAP_OUT:
      handleSwapOutRequest(request);
      break;
    default:
      assert(false && "Unknown swap request type");
    }

    FrameAlloc::instance()->lock_.release();
    InvPageTable::instance()->lock_.release();
  }

  printSwapStatistics();
  swap_request_lock_.release();
}

void SwapEngine::handleSwapOutRequest(SwapReq *request) {
  debug(SWAP, "SwapEngine: Handling SWAP_OUT request\n");

  // set the current swap request
  current_swap_request_ = request;

  // initialize info
  current_swap_request_->info.evicted_ppn = 0;
  current_swap_request_->info.swap_slot_to_write = 0;
  current_swap_request_->info.current_ipt_entry = nullptr;

  markEvictedPPN();
  findSwapSlot();

  // IPT Entry is set in the markedEvictedPPN function

  lockArchmemsInIptEntry(current_swap_request_->info.current_ipt_entry);

  // change the PTEs of all processes that map to this PPN
  // should be marked as swapped out
  for (auto &mapping :
       current_swap_request_->info.current_ipt_entry->mappings_) {
    UserProcess *process = mapping.first;
    if (!process)
      assert(false && "SwapEngine: Null process in IPT entry mappings!");
    for (size_t vpn : mapping.second) {
      ArchMemoryMapping m = process->loader_->arch_memory_.resolveMapping(vpn);
      changePTEToSwappedOut(m.pt[m.pti],
                            current_swap_request_->info.swap_slot_to_write);
      if (process->sharedMemManager()) {
        process->sharedMemManager()->handlePageSwappedOut(
            vpn, current_swap_request_->info.swap_slot_to_write);
      }
    }
  }

  writePageToSwapDevice();

  // Mark request as done and wake up waiting thread
  current_swap_request_->info.dest_ppn =
      current_swap_request_->info.evicted_ppn;
  current_swap_request_->done = true;
  current_swap_request_->info.wait_info.lock.acquire();
  current_swap_request_->info.wait_info.condition.signal();
  current_swap_request_->info.wait_info.lock.release();

  unlockArchmemsInIptEntry(current_swap_request_->info.current_ipt_entry);

  swap_statistic_.swap_out_requests++;

  debug(SWAP, "SwapEngine: SWAP_OUT request handled successfully\n");
}

void SwapEngine::handleSwapInRequest(SwapReq *request) {
  debug(SWAP, "SwapEngine: Handling SWAP_IN request\n");

  // set the current swap request
  current_swap_request_ = request;

  // initialize info
  current_swap_request_->info.swap_slot_to_write = 0;
  current_swap_request_->info.current_ipt_entry = nullptr;

  assert(current_swap_request_->info.evicted_ppn != 0 &&
         "SwapEngine: SWAP_IN request has invalid evicted_ppn!");

  // Swap in the page from swap device
  debug(SWAP, "SwapEngine: Swapping in page from swap device\n");

  current_swap_request_->info.current_ipt_entry =
      getSwappedOutIptEntry(current_swap_request_->info.swap_slot_to_read);

  lockArchmemsInIptEntry(current_swap_request_->info.current_ipt_entry);

  debug(SWAP, "SwapEngine: readPageFromSwapDevice() line\n");
  readPageFromSwapDevice();

  // Reset the physical frame refcount to match the swapped-in page mappings.
  // Otherwise COW can free a still-shared frame after swap-in.
  if (current_swap_request_->info.current_ipt_entry) {
    uint32 new_refcount = static_cast<uint32>(
        current_swap_request_->info.current_ipt_entry->refcount);
    if (new_refcount == 0) {
      new_refcount = 1;
    }
    FrameAlloc::instance()
        ->page_refcounts_[current_swap_request_->info.evicted_ppn] =
        new_refcount;
  }

  // change the PTEs of all processes that map to this PPN
  // should be marked as present
  for (auto &mapping :
       current_swap_request_->info.current_ipt_entry->mappings_) {
    UserProcess *process = mapping.first;
    if (!process)
      assert(false && "SwapEngine: Null process in IPT entry mappings!");
    for (size_t vpn : mapping.second) {
      ArchMemoryMapping m = process->loader_->arch_memory_.resolveMapping(vpn);
      changePTEToPresent(m.pt[m.pti], current_swap_request_->info.evicted_ppn);
      if (process->sharedMemManager()) {
        process->sharedMemManager()->handlePageSwappedIn(
            vpn, current_swap_request_->info.evicted_ppn);
      }
    }
  }

  unlockArchmemsInIptEntry(current_swap_request_->info.current_ipt_entry);

  InvPageTable::instance()->setIPTEntry(
      current_swap_request_->info.evicted_ppn,
      current_swap_request_->info.current_ipt_entry);

  swap_statistic_.swap_in_requests++;

  // Mark request as done and wake up waiting threads
  current_swap_request_->info.dest_ppn =
      current_swap_request_->info.evicted_ppn;
  current_swap_request_->done = true;
  current_swap_request_->info.wait_info.lock.acquire();
  current_swap_request_->info.wait_info.condition.broadcast();
  current_swap_request_->info.wait_info.lock.release();

  debug(SWAP, "SwapEngine: SWAP_IN request handled successfully\n");
}

void SwapEngine::markEvictedPPN() {

  InvPageTableEntry *ipt_entry = nullptr;

  // found IPT Entry can also be free (PD, PT, PTPD pages)
  // or nullptr (if never used before except after initialization)
  while (!ipt_entry || ipt_entry->isFree()) {
    handlePRA();

    ipt_entry = InvPageTable::instance()->getIptEntry(
        current_swap_request_->info.evicted_ppn);

    if (!ipt_entry) {
      debug(SWAP,
            "SwapEngine: Chosen PPN %u has null IPT entry, choosing another "
            "one\n",
            current_swap_request_->info.evicted_ppn);
      continue;
    }

    if (ipt_entry != nullptr && ipt_entry->isFree()) {
      debug(SWAP,
            "SwapEngine: Chosen PPN %u has free IPT entry, choosing another "
            "one, could be PD,PT or PTPD\n",
            current_swap_request_->info.evicted_ppn);
      continue;
    }
  }

  current_swap_request_->info.current_ipt_entry = ipt_entry;

  debug(SWAP, "SwapEngine: Evicted PPN is %u\n",
        current_swap_request_->info.evicted_ppn);
}

void SwapEngine::handlePRA() {
  switch (pra_) {
  case PageReplacementAlgorithm::RANDOM:
    handlePRA_Random();
    break;
  case PageReplacementAlgorithm::AGING:
    handlePRA_Aging();
    break;
  case PageReplacementAlgorithm::SECOND_CHANCE:
    handlePRA_SecondChance();
    break;
  default:
    assert(false && "Unknown Page Replacement Algorithm");
  }
}

void SwapEngine::handlePRA_SecondChance() {
  debug(SWAP, "SwapEngine: Handling PRA SECOND_CHANCE\n");

  // check current pointer entry if referenced
  InvPageTable *ipt = InvPageTable::instance();

  assert(ipt->lock_.isHeldBy(currentThread) &&
         "SwapEngine::handlePRA_SecondChance: IPT lock not held by current "
         "thread!");

  size_t scanned = 0;
  while (true) {
    InvPageTableEntry *entry = ipt->ipt_entries_[current_clock_pointer_];
    if (entry && !entry->isFree()) {
      if (entry->referenced_) {
        debug(SWAP,
              "SwapEngine::handlePRA_SecondChance: PPN %u referenced, "
              "clearing\n",
              current_clock_pointer_);
        entry->referenced_ = false;
      } else {
        // found the victim
        debug(SWAP,
              "SwapEngine::handlePRA_SecondChance: Selected PPN %u after %zu "
              "scans\n",
              current_clock_pointer_, scanned);
        current_swap_request_->info.evicted_ppn = current_clock_pointer_;
        // move the clock pointer to next for next time
        current_clock_pointer_++;
        if (current_clock_pointer_ > ppn_swap_max_)
          current_clock_pointer_ = ppn_swap_min_;
        return;
      }
    }
    // move the clock pointer to next
    current_clock_pointer_++;
    if (current_clock_pointer_ > ppn_swap_max_)
      current_clock_pointer_ = ppn_swap_min_;
    scanned++;
  }
}

void SwapEngine::handlePRA_Random() {
  debug(SWAP, "SwapEngine: Handling PRA RANDOM\n");

  size_t random_number = Scheduler::instance()->getRandomNumberInRange(
      ppn_swap_min_, ppn_swap_max_);
  size_t ppn_to_evict = random_number;
  current_swap_request_->info.evicted_ppn = ppn_to_evict;
}

void SwapEngine::handlePRA_Aging() {
  debug(SWAP, "SwapEngine: Handling PRA AGING\n");

  if (ppn_swap_min_ > ppn_swap_max_) {
    debug(SWAP, "SwapEngine::handlePRA_Aging: invalid swap range\n");
  }

  InvPageTable *ipt = InvPageTable::instance();

  // thread is holding the ipt so no need for locking?
  bool locked_before = ipt->lock_.heldBy() == currentThread;
  if (!locked_before)
    ipt->lock_.acquire();

  uint32 min_age = 0xFFFFFFFFu;
  size_t candidate_ppn = 0;
  bool found = false;

  for (size_t ppn = ppn_swap_min_; ppn <= ppn_swap_max_; ++ppn) {
    InvPageTableEntry *entry = ipt->ipt_entries_[ppn];
    if (!entry || entry->isFree())
      continue;

    bool accessed = false;
    for (const auto &mapping : entry->mappings_) {
      UserProcess *process = mapping.first;
      if (!process || !process->loader_)
        continue;

      ArchMemory &arch_memory = process->loader_->arch_memory_;
      for (size_t vpn : mapping.second) {
        if (arch_memory.isAccessed(vpn)) {
          accessed = true;
          arch_memory.clearAccessed(vpn);
        }
      }
    }

    if (accessed) {
      debug(SWAP,
            "SwapEngine::handlePRA_Aging: PPN %lu accessed, updating age\n",
            ppn);
    }

    entry->age_counter_ >>= 1;
    if (accessed)
      entry->age_counter_ = entry->age_counter_ | 0x80000000u;

    if (!found || entry->age_counter_ < min_age) {
      min_age = entry->age_counter_;
      candidate_ppn = ppn;
      found = true;
      debug(SWAP,
            "SwapEngine::handlePRA_Aging: New candidate PPN %lu (age 0x%x)\n",
            candidate_ppn, min_age);
    } else if (entry->age_counter_ == min_age) {
      if (Scheduler::instance()->getRandomNumberInRange(0, 1) == 0)
        candidate_ppn = ppn;
    }
  }

  if (!locked_before)
    ipt->lock_.release();

  assert(found && "SwapEngine::handlePRA_Aging: no eviction candidate found");
  current_swap_request_->info.evicted_ppn = candidate_ppn;
}

void SwapEngine::findSwapSlot() {
  debug(SWAP, "SwapEngine: Finding free swap slot\n");

  assert(swap_device_bitmap_->getNumFreeBits() > 0 &&
         "SwapEngine: No free swap slots available!");

  // TODO: Add logic to reuse freed swap slots, not so important for now
  size_t free_slot = 0;

  while (swap_device_bitmap_->getBit(free_slot)) {
    free_slot++;
    assert(free_slot < swap_device_bitmap_->getSize() &&
           "SwapEngine: No free swap slots available!");
  }

  current_swap_request_->info.swap_slot_to_write = free_slot;
}

//sum of values in that page
uint32 SwapEngine::compute_page_checksum(uint32 ppn) {
  uint32 sum = 0;
  uint32 *p = (uint32 *)ArchMemory::getIdentAddressOfPPN(ppn);
  for (size_t i = 0; i < PAGE_SIZE / sizeof(uint32); ++i)
    sum += p[i];

  return sum;
}

void SwapEngine::writePageToSwapDevice() {
  debug(SWAP, "SwapEngine: Writing page to swap device\n");

  size_t offeset = current_swap_request_->info.swap_slot_to_write * PAGE_SIZE;
  size_t ident_address =
      ArchMemory::getIdentAddressOfPPN(current_swap_request_->info.evicted_ppn);

  assert(swap_device_->writeData(offeset, PAGE_SIZE, (char *)ident_address) &&
         "Failed to write page to swap device!");

  debug(SWAP, "SwapEngine: Writing page to checksum array\n");
  swap_slot_checksums_[current_swap_request_->info.swap_slot_to_write] =
      compute_page_checksum(current_swap_request_->info.evicted_ppn);

  // reset the page to free pattern
  memset((void *)ident_address, 0xFF, PAGE_SIZE);

  // Save the swap slot as used and add info to the array
  swap_device_bitmap_->setBit(current_swap_request_->info.swap_slot_to_write);

  // Save PTE entry to swap info map
  InvPageTableEntry *ipt_entry =
      current_swap_request_->info.current_ipt_entry;

  swapped_out_pages_entries_[current_swap_request_->info.swap_slot_to_write] =
      ipt_entry;

  // set the old IPT entry to  null since it's swapped out
  InvPageTable::instance()->setEntryToNull(
      current_swap_request_->info.evicted_ppn);

  swap_statistic_.pages_swapped_out++;

  debug(SWAP,
        "SwapEngine: written to swap device, evicted PPN %u written to swap "
        "slot "
        "%zu\n",
        current_swap_request_->info.evicted_ppn,
        current_swap_request_->info.swap_slot_to_write);
}

void SwapEngine::readPageFromSwapDevice() {
  debug(SWAP, "SwapEngine: Reading page from swap device\n");

  size_t offeset = current_swap_request_->info.swap_slot_to_read * PAGE_SIZE;
  size_t ident_address =
      ArchMemory::getIdentAddressOfPPN(current_swap_request_->info.evicted_ppn);

  assert(swap_device_->readData(offeset, PAGE_SIZE, (char *)ident_address) &&
         "Failed to read page from swap device!");

  uint32 expected_checksum = swap_slot_checksums_[current_swap_request_->info.swap_slot_to_read];
  uint32 actual_checksum = compute_page_checksum(current_swap_request_->info.evicted_ppn);
  if (expected_checksum != actual_checksum)
  {
    debug(SWAP, "SwaaaaaaaaaaaaaaaaapManager: checksum mismatch for swap slot %zu (expected %u, got %u)\n",
      current_swap_request_->info.swap_slot_to_read, expected_checksum, actual_checksum);
  }
  else
  {
    debug(SWAP, "SwaaaaaaaaaaaaaaaaapManager: checksum is the same for swap slot %zu (expected %u, got %u)\n",
      current_swap_request_->info.swap_slot_to_read, expected_checksum, actual_checksum);
  }

  // Mark the swap slot as free and clear its IPT mapping pointer
  size_t freed_slot = current_swap_request_->info.swap_slot_to_read;
  swap_device_bitmap_->unsetBit(freed_slot);
  swapped_out_pages_entries_[freed_slot] = nullptr;
  swap_slot_checksums_[freed_slot] = 0;

  swap_statistic_.pages_swapped_in++;

  debug(SWAP,
        "SwapEngine: read from swap device, swap slot %zu read into evicted "
        "PPN %u\n",
        current_swap_request_->info.swap_slot_to_read,
        current_swap_request_->info.evicted_ppn);
}

void SwapEngine::changePTEToSwappedOut(PageTableEntry &pte, size_t swap_slot) {
  // change the PTE to swapped out
  // set present to 0, swapped_out to 1, and page_ppn to swap_slot

  pte.present = 0;
  pte.swapped_out = 1;
  pte.page_ppn = swap_slot;
}

void SwapEngine::changePTEToPresent(PageTableEntry &pte, uint32 ppn) {
  // change the PTE to present
  // set present to 1, swapped_out to 0, and page_ppn to the new ppn

  pte.present = 1;
  pte.swapped_out = 0;
  pte.page_ppn = ppn;
}

void SwapEngine::lockArchmemsInIptEntry(InvPageTableEntry *ipt_entry) {
  assert(ipt_entry &&
         "SwapEngine: Null IPT entry in unlockArchmemsInIptEntry!");
  debug(SWAP, "SwapEngine: Locking arch memories in IPT entry\n");
  for (auto &mapping : ipt_entry->mappings_) {
    UserProcess *process = mapping.first;
    if (!process)
      assert(false && "SwapEngine: Null process in IPT entry mappings!");
    debug(SWAP, "SwapEngine: Locking arch memory of process PID %zu, %s\n",
          process->getPID(), process->minixfs_filename_.c_str());
    assert(process->loader_ &&
           "SwapEngine: Null loader in process in IPT entry mappings!");
    assert(currentThread &&
           "SwapEngine: Null current thread when locking arch memories!");
    assert(!process->loader_->arch_memory_.arch_memory_lock_.isHeldBy(
               currentThread) &&
           "SwapEngine: Arch memory lock already held by current thread!");
    process->loader_->arch_memory_.arch_memory_lock_.acquire();
  }
}

void SwapEngine::unlockArchmemsInIptEntry(InvPageTableEntry *ipt_entry) {
  assert(ipt_entry &&
         "SwapEngine: Null IPT entry in unlockArchmemsInIptEntry!");
  debug(SWAP, "SwapEngine: Unlocking arch memories in IPT entry\n");
  for (auto &mapping : ipt_entry->mappings_) {
    UserProcess *process = mapping.first;
    if (!process)
      assert(false && "SwapEngine: Null process in IPT entry mappings!");
    debug(SWAP, "SwapEngine: Unlocking arch memory of process PID %zu, %s\n",
          process->getPID(), process->minixfs_filename_.c_str());
    assert(process->loader_ &&
           "SwapEngine: Null loader in process in IPT entry mappings!");
    assert(currentThread &&
           "SwapEngine: Null current thread when unlocking arch memories!");
    assert(process->loader_->arch_memory_.arch_memory_lock_.isHeldBy(
               currentThread) &&
           "SwapEngine: Arch memory lock not held by current thread!");
    process->loader_->arch_memory_.arch_memory_lock_.release();
  }
}

bool SwapEngine::isSwapRequestQueueEmpty() {
  assert(swap_request_lock_.isHeldBy(currentThread) &&
         "SwapEngine::isSwapRequestQueueEmpty: swap_request_lock_ not held!");
  bool is_empty =
      (swap_requests_head_ == nullptr) && (swap_requests_tail_ == nullptr);
  return is_empty;
}

// this is a little bit confusing, but I didnt move the
// swapped_out_pages_entries_ to the IPT manager
// thats why we need to hold the IPT lock here
InvPageTableEntry *SwapEngine::getSwappedOutIptEntry(size_t swap_slot) {
  assert(InvPageTable::instance()->lock_.isHeldBy(currentThread) &&
         "SwapEngine::getSwappedOutIptEntry: IPT lock not held by current "
         "thread!");
  assert(swap_slot < swap_device_->getNumBlocks() &&
         "SwapEngine: Swap slot out of range in getSwappedOutIptEntry!");

  return swapped_out_pages_entries_[swap_slot];
}

void SwapEngine::resetPRAData() {
  debug(SWAP, "SwapEngine: Resetting PRA data\n");

  InvPageTable *ipt = InvPageTable::instance();

  assert(ipt->lock_.isHeldBy(currentThread) &&
         "SwapEngine::resetPRAData: IPT lock not held by current thread!");

  switch (pra_) {
  case PageReplacementAlgorithm::RANDOM:
    // No data to reset for RANDOM algorithm
    break;
  case PageReplacementAlgorithm::AGING: {
    for (size_t ppn = ppn_swap_min_; ppn <= ppn_swap_max_; ++ppn) {
      InvPageTableEntry *entry = ipt->ipt_entries_[ppn];
      if (!entry || entry->isFree())
        continue;

      entry->age_counter_ = 0;
    }
  } break;
  case PageReplacementAlgorithm::SECOND_CHANCE: {
    current_clock_pointer_ = ppn_swap_min_;
    for (size_t ppn = ppn_swap_min_; ppn <= ppn_swap_max_; ++ppn) {
      InvPageTableEntry *entry = ipt->ipt_entries_[ppn];
      if (!entry || entry->isFree())
        continue;

      entry->referenced_ = false;
    }

  } break;
  default:
    assert(false && "Unknown Page Replacement Algorithm");
  }
}

void SwapEngine::setCurrentPRA(PageReplacementAlgorithm pra) {
  debug(SWAP, "SwapEngine: Setting current PRA to %d\n", (int)pra);

  swap_request_lock_.acquire();
  InvPageTable::instance()->lock_.acquire();
  FrameAlloc::instance()->lock_.acquire();

  pra_ = pra;
  resetPRAData();

  FrameAlloc::instance()->lock_.release();
  InvPageTable::instance()->lock_.release();
  swap_request_lock_.release();
}

void SwapEngine::checkForPreSwappingNeeds() {
  swap_request_lock_.acquire();
  FrameAlloc::instance()->lock_.acquire();
  InvPageTable::instance()->lock_.acquire();

  uint32 free_pages = FrameAlloc::instance()->getNumFreePages();

  if (free_pages < PRE_SWAPPING_FREE_PAGE_THRESHOLD) {
    debug(SWAP,
          "SwapEngine: Free pages (%u) below threshold (%u), scheduling "
          "pre-swapping\n",
          free_pages, PRE_SWAPPING_FREE_PAGE_THRESHOLD);
    schedule_swapping_thread_ = true;
    pre_swapping_active_ = true;
  }

  InvPageTable::instance()->lock_.release();
  FrameAlloc::instance()->lock_.release();
  swap_request_lock_.release();
}

void SwapEngine::handlePreSwapping() {

  if (!pre_swapping_active_)
    return;

  debug(SWAP, "SwapEngine: Handling pre-swapping\n");
  FrameAlloc::instance()->lock_.acquire();
  InvPageTable::instance()->lock_.acquire();

  size_t free_pages = FrameAlloc::instance()->getNumFreePages();
  size_t i = 0;
  size_t threshold = 26;

  while (free_pages < (PRE_SWAPPING_FREE_PAGE_THRESHOLD + threshold)) {
    i++;
    free_pages = FrameAlloc::instance()->getNumFreePages();
    debug(SWAP, "SwapEngine: Pre-swapping page %zu/%u\n", i + 1,
          PRE_SWAPPING_FREE_PAGE_THRESHOLD);

    current_swap_request_ = current_pre_swap_request_;

    // initialize info
    current_pre_swap_request_->info.evicted_ppn = 0;
    current_pre_swap_request_->info.swap_slot_to_write = 0;
    current_pre_swap_request_->info.current_ipt_entry = nullptr;

    markEvictedPPN();
    findSwapSlot();

    // IPT Entry is set in the markedEvictedPPN function
    lockArchmemsInIptEntry(current_pre_swap_request_->info.current_ipt_entry);

    // change the PTEs of all processes that map to this PPN
    // should be marked as swapped out
    for (auto &mapping :
         current_pre_swap_request_->info.current_ipt_entry->mappings_) {
      UserProcess *process = mapping.first;
      if (!process)
        assert(false && "SwapEngine: Null process in IPT entry mappings!");
      for (size_t vpn : mapping.second) {
        ArchMemoryMapping m =
            process->loader_->arch_memory_.resolveMapping(vpn);
        changePTEToSwappedOut(
            m.pt[m.pti], current_pre_swap_request_->info.swap_slot_to_write);
        if (process->sharedMemManager()) {
          process->sharedMemManager()->handlePageSwappedOut(
              vpn, current_pre_swap_request_->info.swap_slot_to_write);
        }
      }
    }

    writePageToSwapDevice();

    InvPageTable::instance()->setEntryToNull(
        current_pre_swap_request_->info.evicted_ppn);

    FrameAlloc::instance()->markPPNAsFree(
        current_pre_swap_request_->info.evicted_ppn);

    unlockArchmemsInIptEntry(current_pre_swap_request_->info.current_ipt_entry);
  }

  InvPageTable::instance()->lock_.release();
  FrameAlloc::instance()->lock_.release();
  debug(SWAP, "SwapEngine: Pre-swapping completed\n");
  pre_swapping_active_ = false;
}
