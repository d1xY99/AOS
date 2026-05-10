#pragma once

#include "SpinLock.h"
#include "SwapEngine.h"
#include "SwappingThread.h"
#include "paging-definitions.h"
#include "types.h"

#define DYNAMIC_KMM                                                            \
  (0) // Please note that this means that the KMM depends on the page manager
// and you will have a harder time implementing swapping. Pros only!

class Bitmap;

class FrameAlloc {

  friend class SwapEngine;
  friend class SwappingThread;

public:
  static FrameAlloc *instance();

  /**
   * Returns the total number of physical pages available to AOS.
   * @return Number of available physical pages
   */
  uint32 getTotalNumPages() const;

  /**
   * Returns the number of currently free physical pages.
   * @return Number of free physical pages
   */
  uint32 getNumFreePages() const;

  /**
   * Marks the lowest free physical page as used and returns it.
   * Always returns 4KB PPNs.
   * @param page_size The requested page size, must be a multiple of PAGE_SIZE
   * @return The allocated physical page PPN.
   */
  uint32 allocPPN(uint32 page_size = PAGE_SIZE);

  /**
   * Marks the given physical page as free
   * @param page_number The physical page PPN to free
   * @param page_size The page size to free, must be a multiple of PAGE_SIZE
   */
  void freePPN(uint32 page_number, uint32 page_size = PAGE_SIZE);

  /**
   * Increases the reference count of the given physical page.
   * Does nothing for invalid/sentinel PPNs.
   */
  void increfPPN(uint32 page_number);

  Thread *heldBy() { return lock_.heldBy(); }

  FrameAlloc();

  void printBitmap();

  uint32 getNumPagesForUser() const;

  void markPPNAsFree(uint32 ppn, uint32 page_size = PAGE_SIZE);

private:
  bool reservePages(uint32 ppn, uint32 num = 1);

  FrameAlloc(FrameAlloc const &);

  Bitmap *page_usage_table_;
  uint32 number_of_pages_;
  uint32 lowest_unreserved_page_;
  uint32 num_pages_for_user_;
  uint32 *page_refcounts_; // tracks how many active mappings refer to each
                           // physical page

  SpinLock lock_;

  static FrameAlloc *instance_;

  size_t HEAP_PAGES;
};
