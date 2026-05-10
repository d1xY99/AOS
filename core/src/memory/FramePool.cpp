#include "FramePool.h"
#include "debug.h"

FramePool::FramePool(size_t count, uint32 page_size)
    : page_size_(page_size) {
  pages_.reserve(count);
  debug(A_MEMORY,
        "FramePool: Preallocating %zu pages of size %u bytes.\n",
        count, page_size_);
  for (size_t i = 0; i < count; ++i) {
    uint32 ppn = FrameAlloc::instance()->allocPPN(page_size_);
    if (ppn == 0)
      break;
    pages_.push_back(ppn);
  }
}

FramePool::~FramePool() {
  if (size() > 0)
    assert(false && "FramePool not fully freed before "
                    "destruction, memory leak!");
}

uint32 FramePool::pop() {
  if (pages_.empty()) {
    assert(false && "FramePool underflow, we ran out of "
                    "preallocated pages, increase the pool size!");
  }
  uint32 ppn = pages_.back();
  pages_.pop_back();
  debug(A_MEMORY,
        "FramePool: Popped page ppn %u, %zu pages remaining.\n", ppn,
        pages_.size());
  return ppn;
}

void FramePool::freeAll() {
  debug(A_MEMORY,
        "FramePool: Freeing all %zu preallocated pages of size %u "
        "bytes.\n",
        pages_.size(), page_size_);
  for (size_t i = 0; i < pages_.size(); ++i)
    FrameAlloc::instance()->freePPN(pages_[i], page_size_);
  pages_.clear();
}
