
#include "SwappingThread.h"
#include "ArchCommon.h"
#include "FrameAlloc.h"
#include "Scheduler.h"
#include "SwapEngine.h"
#include "assert.h"
#include "debug.h"
#include "kernel_user_shared.h"

SwappingThread::SwappingThread()
    : Thread(0, "SwappingThread", Thread::KERNEL_THREAD) {
  debug(SWAP, "SwappingThread::SwappingThread: created\n");
}

void SwappingThread::Run() {
  while (1) {

    assert(SWAP_ENABLED && "SwappingThread::Run: Swapping is not enabled!");

    if (PRE_SWAP_ENABLED) {
      SwapEngine::instance()->checkForPreSwappingNeeds();
      SwapEngine::instance()->handlePreSwapping();
    }

    SwapEngine::instance()->handleSwapRequests();

    // disable scheduling if not allowed
    SwapEngine::instance()->swap_request_lock_.acquire();
    if (SwapEngine::instance()->isSwapRequestQueueEmpty()) {
      SwapEngine::instance()->schedule_swapping_thread_ = false;
    }
    SwapEngine::instance()->swap_request_lock_.release();

    Scheduler::instance()->yield();
  }
}

// Checks in the scheduler whether this thread is allowed to be scheduled
bool SwappingThread::schedulable() {
  if (PRE_SWAP_ENABLED)
    return true;
  else
    return SwapEngine::instance()->schedule_swapping_thread_;
}
