#pragma once

#include "BlockDevTable.h"
#include "BlockDev.h"
#include "Bitmap.h"
#include "Condition.h"
#include "InvPageTable.h"
#include "Mutex.h"
#include "SwappingThread.h"
#include "Thread.h"
#include "paging-definitions.h"
#include "types.h"

/* Swapping
 *
 * -> [1] A thread needs to allocate memory but there is none free
 *  1) Thread tries to allocate a new page in allocPPN()
 *  2) allocPPN() has not enough free memory pages
 *  3) It creates a SwapReq and adds it to the SwappingThread
 *  4) Thread needs to wait until the SwappingThread processed the request
 *  5) GOTO [2]
 *  6) After the SwappingThread processed the request, the thread is woken up
 *  7) Thread retries to allocate memory in allocPPN()
 *
 *
 * -> [2] The SwappingThread processes SwapRequests
 *
 *
 *
 *  -> [3] Thread wait logic
 *  1) Thread creates a WaitingOnSwap structure
 *  2) Adds it to the Swap request
 *  3) Locks the WaitingOnSwap lock
 *  4) Adds the SwapReq to the SwappingThread queue
 *  5) Waits on the WaitingOnSwap condition
 */

#define SWAP_DEVICE_NR 3

enum PageReplacementAlgorithm {
  RANDOM, // simple random page replacement
  AGING,
  SECOND_CHANCE,
};

enum SWAP_TYPE { SWAP_IN, SWAP_OUT, PRE_SWAP_OUT };

struct SwapWait {
  Mutex lock;
  Condition condition;

  SwapWait()
      : lock("SwapWait::Lock"), condition(&lock, "SwapWait::Cond") {}
};

struct SwapReqInfo {
  // if SWAP_IN
  size_t vpn;               // virtual page number to swap in
  size_t dest_ppn;          // destination physical page number
  size_t swap_slot_to_read; // swap slot to read from
  // if SWAP_OUT
  size_t src_ppn;            // source physical page number
  size_t swap_slot_to_write; // swap slot to write to
  uint32 evicted_ppn;        // evicted physical page number

  InvPageTableEntry *current_ipt_entry;

  SwapWait wait_info;

  SwapReqInfo()
      : vpn(0), dest_ppn(0), swap_slot_to_read(0), src_ppn(0),
        swap_slot_to_write(0), evicted_ppn(0), wait_info() {}
};

struct SwapReq {
  SWAP_TYPE type;
  SwapReqInfo info;
  bool done;
  size_t waiters; // number of threads waiting on this request

  SwapReq(SWAP_TYPE type_) : type(type_), info(), done(false), waiters(0) {}
};

struct SwapReqNode {
  SwapReq *req;
  SwapReqNode *next;
  SwapReqNode(SwapReq *r) : req(r), next(nullptr) {}
};

struct SwapStats {
  size_t swap_in_requests;
  size_t swap_out_requests;
  size_t pages_swapped_in;
  size_t pages_swapped_out;
};

class SwapEngine {

  friend class SwappingThread;

public:
  static SwapEngine *instance();

  SwapEngine();
  ~SwapEngine();

  // Called when a thread needs memory but there is none free
  // Wait until memory is available again
  // and get the free ppn after that
  uint32 handleThreadNeedsFreePage();

  // Called when a page is swapped out
  // Swaps in the page from swap device
  // and lets the caller wait until done
  void handlePageIsSwappedOut(size_t swap_slot, uint32 new_ppn);

  void addSwapRequest(SwapReq *request);
  void addSwapRequestNode(SwapReq *request);
  SwapReq *popSwapRequestFront();
  void handleSwapRequests();
  void handleSwapInRequest(SwapReq *request);
  void handleSwapOutRequest(SwapReq *request);

  void markEvictedPPN();
  void findSwapSlot();
  void writePageToSwapDevice();
  void readPageFromSwapDevice();

  void handlePRA();
  void handlePRA_Random();
  void handlePRA_Aging();
  void handlePRA_SecondChance();

  void lockArchmemsInIptEntry(InvPageTableEntry *ipt_entry);
  void unlockArchmemsInIptEntry(InvPageTableEntry *ipt_entry);

  bool isSwapRequestQueueEmpty();

  const char *swapTypeToString(SWAP_TYPE type);

  void changePTEToSwappedOut(PageTableEntry &pte, size_t swap_slot);
  void changePTEToPresent(PageTableEntry &pte, uint32 ppn);

  void resetPRAData();

  void setCurrentPRA(PageReplacementAlgorithm pra);

  void checkForPreSwappingNeeds();
  void handlePreSwapping();

  void printSwapStatistics();

  static uint32 compute_page_checksum(uint32 ppn);

  InvPageTableEntry *getSwappedOutIptEntry(size_t swap_slot);

  Mutex swap_request_lock_;
  PageReplacementAlgorithm pra_;
  bool schedule_swapping_thread_;
  bool pre_swapping_active_;
  SwapStats swap_statistic_;
  SwapReqNode *swap_requests_head_;
  SwapReqNode *swap_requests_tail_;
  SwapReq *current_swap_request_;
  SwapReq *current_pre_swap_request_;

  InvPageTableEntry **swapped_out_pages_entries_;

  uint32 *swap_slot_checksums_;

private:
  static SwapEngine *instance_;

  Bitmap *swap_device_bitmap_;
  BlockDev *swap_device_;

  uint32 ppn_swap_min_;
  uint32 ppn_swap_max_;
  uint32 current_clock_pointer_;
};
