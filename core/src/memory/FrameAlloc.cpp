#include "FrameAlloc.h"
#include "ArchCommon.h"
#include "ArchMemory.h"
#include "Bitmap.h"
#include "InvPageTable.h"
#include "KernelHeap.h"
#include "Scheduler.h"
#include "SwapEngine.h"
#include "Thread.h"
#include "StackAllocator.h"
#include "assert.h"
#include "debug.h"
#include "kprintf.h"
#include "new.h"
#include "offsets.h"
#include "paging-definitions.h"
#include <assert.h>

FrameAlloc pm;

FrameAlloc *FrameAlloc::instance_ = nullptr;

FrameAlloc *FrameAlloc::instance() {
  if (unlikely(!instance_))
    new (&pm) FrameAlloc();
  return instance_;
}

FrameAlloc::FrameAlloc() : lock_("FrameAlloc::lock_") {
  assert(!instance_);
  instance_ = this;
  assert(!KernelHeap::instance_);
  number_of_pages_ = 0;
  lowest_unreserved_page_ = 0;

  size_t num_mmaps = ArchCommon::getNumUseableMemoryRegions();

  size_t highest_address = 0, used_pages = 0;

  // Determine Amount of RAM
  for (size_t i = 0; i < num_mmaps; ++i) {
    pointer start_address = 0, end_address = 0, type = 0;
    ArchCommon::getUsableMemoryRegion(i, start_address, end_address, type);
    debug(PM,
          "Ctor: memory region from physical %zx to %zx (%zu bytes) of type "
          "%zd\n",
          start_address, end_address, end_address - start_address, type);

    if (type == 1)
      highest_address = Max(highest_address, end_address & 0x7FFFFFFF);
  }

  number_of_pages_ = highest_address / PAGE_SIZE;

  size_t boot_bitmap_size = Min(4096 * 8 * 2, number_of_pages_);
  uint8 page_usage_table[BITMAP_BYTE_COUNT(boot_bitmap_size)];
  used_pages = boot_bitmap_size;
  memset(page_usage_table, 0xFF, BITMAP_BYTE_COUNT(boot_bitmap_size));

  // mark as free, everything that might be useable
  for (size_t i = 0; i < num_mmaps; ++i) {
    pointer start_address = 0, end_address = 0, type = 0;
    ArchCommon::getUsableMemoryRegion(i, start_address, end_address, type);
    if (type != 1)
      continue;
    size_t start_page = start_address / PAGE_SIZE;
    size_t end_page = end_address / PAGE_SIZE;
    debug(PM,
          "Ctor: usable memory region: start_page: %zx, end_page: %zx, type: "
          "%zd\n",
          start_page, end_page, type);

    for (size_t k = Max(start_page, lowest_unreserved_page_);
         k < Min(end_page, number_of_pages_); ++k) {
      Bitmap::unsetBit(page_usage_table, used_pages, k);
    }
  }

  debug(PM, "Ctor: Marking pages used by the kernel as reserved\n");
  for (size_t i = ArchMemory::RESERVED_START; i < ArchMemory::RESERVED_END;
       ++i) {
    size_t physical_page = 0;
    size_t pte_page = 0;
    size_t this_page_size = ArchMemory::get_PPN_Of_VPN_In_KernelMapping(
        i, &physical_page, &pte_page);
    assert(this_page_size == 0 || this_page_size == PAGE_SIZE ||
           this_page_size == PAGE_SIZE * PAGE_TABLE_ENTRIES);
    if (this_page_size > 0) {
      // our bitmap only knows 4k pages for now
      uint64 num_4kpages =
          this_page_size /
          PAGE_SIZE; // should be 1 on 4k pages and 1024 on 4m pages
      for (uint64 p = 0; p < num_4kpages; ++p) {
        if (physical_page * num_4kpages + p < number_of_pages_)
          Bitmap::setBit(page_usage_table, used_pages,
                         physical_page * num_4kpages + p);
      }
      i += (num_4kpages - 1); //+0 in most cases

      if (num_4kpages == 1 && i % 1024 == 0 && pte_page < number_of_pages_)
        Bitmap::setBit(page_usage_table, used_pages, pte_page);
    }
  }

  debug(PM, "Ctor: Marking GRUB loaded modules as reserved\n");
  // LastbutNotLeast: Mark Modules loaded by GRUB as reserved (i.e. pseudofs,
  // etc)
  for (size_t i = 0; i < ArchCommon::getNumModules(); ++i) {
    size_t start_page =
        (ArchCommon::getModuleStartAddress(i) & ~IDENT_MAPPING_START) /
        PAGE_SIZE;
    size_t end_page =
        (ArchCommon::getModuleEndAddress(i) & ~IDENT_MAPPING_START) / PAGE_SIZE;
    debug(PM, "Ctor: module: start_page: %zx, end_page: %zx\n", start_page,
          end_page);
    for (size_t k = Min(start_page, number_of_pages_);
         k <= Min(end_page, number_of_pages_ - 1); ++k)
      Bitmap::setBit(page_usage_table, used_pages, k);
  }

  size_t num_pages_for_bitmap = (number_of_pages_ / 8) / PAGE_SIZE + 1;
  assert(used_pages < number_of_pages_ / 2 && "No space for kernel heap!");

  HEAP_PAGES = number_of_pages_ / 2 - used_pages;
  if (HEAP_PAGES > 1024)
    HEAP_PAGES = 1024 + (HEAP_PAGES - Min(HEAP_PAGES, 1024)) / 8;

  size_t start_vpn = ArchCommon::getFreeKernelMemoryStart() / PAGE_SIZE;
  size_t free_page = 0;
  size_t temp_page_size = 0;
  size_t num_reserved_heap_pages = 0;
  for (num_reserved_heap_pages = 0;
       num_reserved_heap_pages < num_pages_for_bitmap || temp_page_size != 0 ||
       num_reserved_heap_pages <
           ((DYNAMIC_KMM || (number_of_pages_ < 512)) ? 0 : HEAP_PAGES);
       ++num_reserved_heap_pages) {
    while (!Bitmap::setBit(page_usage_table, used_pages, free_page))
      free_page++;
    if ((temp_page_size =
             ArchMemory::get_PPN_Of_VPN_In_KernelMapping(start_vpn, 0, 0)) == 0)
      ArchMemory::mapKernelPage(start_vpn, free_page++);
    start_vpn++;
  }

  extern KernelHeap kmm;
  new (&kmm) KernelHeap(num_reserved_heap_pages, HEAP_PAGES);
  page_usage_table_ = new Bitmap(number_of_pages_);

  for (size_t i = 0; i < boot_bitmap_size; ++i) {
    if (Bitmap::getBit(page_usage_table, i))
      page_usage_table_->setBit(i);
  }

  // track how many virtual mappings refer to every physical page so COW can
  // share frames safely
  page_refcounts_ = new uint32[number_of_pages_];
  for (uint32 p = 0; p < number_of_pages_; ++p) {
    page_refcounts_[p] = page_usage_table_->getBit(p) ? 1u : 0u;
  }

  debug(PM, "Ctor: find lowest unreserved page\n");
  for (size_t p = 0; p < number_of_pages_; ++p) {
    if (!page_usage_table_->getBit(p)) {
      lowest_unreserved_page_ = p;
      break;
    }
  }
  debug(PM, "Ctor: Physical pages - free: %zu used: %zu total: %u\n",
        page_usage_table_->getNumFreeBits(), page_usage_table_->getNumBitsSet(),
        number_of_pages_);
  assert(lowest_unreserved_page_ < number_of_pages_);

  debug(PM, "Clearing free pages\n");
  for (size_t p = lowest_unreserved_page_; p < number_of_pages_; ++p) {
    if (!page_usage_table_->getBit(p)) {
      memset((void *)ArchMemory::getIdentAddressOfPPN(p), 0xFF, PAGE_SIZE);
    }
  }

  num_pages_for_user_ = DYNAMIC_KMM ? -1 : getNumFreePages();
  KernelHeap::pm_ready_ = 1;

  // make sure Inverted Page Table Manager is created after Page Manager
  InvPageTable::instance();
}

uint32 FrameAlloc::getTotalNumPages() const { return number_of_pages_; }

uint32 FrameAlloc::getNumFreePages() const {
  return page_usage_table_->getNumFreeBits();
}

bool FrameAlloc::reservePages(uint32 ppn, uint32 num) {
  assert(lock_.heldBy() == currentThread);
  if (ppn < number_of_pages_ && !page_usage_table_->getBit(ppn)) {
    if (num == 1 || reservePages(ppn + 1, num - 1)) {
      page_usage_table_->setBit(ppn);
      return true;
    }
  }
  return false;
}

uint32 FrameAlloc::allocPPN(uint32 page_size) {
  uint32 p;
  uint32 found = 0;

  assert((page_size % PAGE_SIZE) == 0);

  lock_.acquire();

  for (p = lowest_unreserved_page_; !found && (p < number_of_pages_); ++p) {
    if ((p % (page_size / PAGE_SIZE)) != 0)
      continue;
    if (reservePages(p, page_size / PAGE_SIZE))
      found = p;
  }
  while ((lowest_unreserved_page_ < number_of_pages_) &&
         page_usage_table_->getBit(lowest_unreserved_page_))
    ++lowest_unreserved_page_;

  if (found != 0) {
    uint32 pages = page_size / PAGE_SIZE;
    for (uint32 offset = 0; offset < pages; ++offset) {
      // brand-new allocation starts out with a single owner
      page_refcounts_[found + offset] = 1;
    }
  }

  lock_.release();

  if (found == 0) {

    assert(SWAP_ENABLED && "FrameAlloc::allocPPN: Out of memory / No more "
                           "free physical pages SWAP IS DISABLED");

    found = SwapEngine::instance()->handleThreadNeedsFreePage();
    debug(PM, "allocPPN: Got page %u from SwapEngine\n", found);

    if (found != 0) {
      lock_.acquire();
      uint32 pages = page_size / PAGE_SIZE;
      for (uint32 offset = 0; offset < pages; ++offset) {
        // brand-new allocation starts out with a single owner
        page_refcounts_[found + offset] = 1;
      }
      lock_.release();
      goto ppn_found;
    }

    assert(
        false &&
        "FrameAlloc::allocPPN: Out of memory / No more free physical pages");
  }

ppn_found:

  const char *page_ident_addr =
      (const char *)ArchMemory::getIdentAddressOfPPN(found);
  const char *page_modified =
      (const char *)memnotchr(page_ident_addr, 0xFF, page_size);
  if (page_modified) {
    debug(PM, "Detected use-after-free for PPN %x at offset %zx\n", found,
          page_modified - page_ident_addr);
    assert(!page_modified && "Page modified after free");
  }

  memset((void *)ArchMemory::getIdentAddressOfPPN(found), 0, page_size);
  return found;
}

void FrameAlloc::freePPN(uint32 page_number, uint32 page_size) {
  debug(PM, "Freeing PPN %u (=%x) of size %u bytes\n", page_number, page_number,
        page_size);
  assert((page_size % PAGE_SIZE) == 0);
  if (page_number >= getTotalNumPages()) {
    debug(PM, "Tried to free PPN %u (=%x)\n", page_number, page_number);
    assert(false && "PPN to be freed is out of range");
  }

  lock_.acquire();
  for (uint32 offset = 0; offset < page_size / PAGE_SIZE; ++offset) {
    uint32 ppn = page_number + offset;
    if (ppn >= number_of_pages_)
      continue;
    assert(page_usage_table_->getBit(ppn) && "Double free PPN");
    assert(page_refcounts_[ppn] > 0 && "PPN refcount mismatch");
    // decrease the refcount, and only free the frame if it reaches zero
    if (--page_refcounts_[ppn] == 0) {
      // last mapping disappeared, make the frame available again
      memset((void *)ArchMemory::getIdentAddressOfPPN(ppn), 0xFF, PAGE_SIZE);
      page_usage_table_->unsetBit(ppn);
      if (ppn < lowest_unreserved_page_)
        lowest_unreserved_page_ = ppn;
    }
  }
  lock_.release();
}

void FrameAlloc::markPPNAsFree(uint32 page_number, uint32 page_size) {
  debug(PM, "Freeing PPN %u (=%x) of size %u bytes\n", page_number, page_number,
        page_size);
  assert((page_size % PAGE_SIZE) == 0);
  assert(lock_.isHeldBy(currentThread) &&
         "markPPNAsFree called without holding the lock");
  if (page_number >= getTotalNumPages()) {
    debug(PM, "Tried to free PPN %u (=%x)\n", page_number, page_number);
    assert(false && "PPN to be freed is out of range");
  }

  for (uint32 offset = 0; offset < page_size / PAGE_SIZE; ++offset) {
    uint32 ppn = page_number + offset;
    if (ppn >= number_of_pages_)
      continue;
    assert(page_usage_table_->getBit(ppn) && "Double free PPN");
    assert(page_refcounts_[ppn] > 0 && "PPN refcount mismatch");
    // last mapping disappeared, make the frame available again
    memset((void *)ArchMemory::getIdentAddressOfPPN(ppn), 0xFF, PAGE_SIZE);
    page_usage_table_->unsetBit(ppn);
    if (ppn < lowest_unreserved_page_)
      lowest_unreserved_page_ = ppn;
  }
}

void FrameAlloc::printBitmap() { page_usage_table_->bmprint(); }

uint32 FrameAlloc::getNumPagesForUser() const { return num_pages_for_user_; }

void FrameAlloc::increfPPN(uint32 page_number) {
  if (page_number >= number_of_pages_)
    return;
  lock_.acquire();
  // bump the refcount before another address space points at the same frame
  assert(page_usage_table_->getBit(page_number) && "increfPPN on free page");
  assert(page_refcounts_[page_number] > 0 && "PPN refcount is zero");
  ++page_refcounts_[page_number];
  lock_.release();
}
