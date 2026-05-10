#pragma once

#include "FrameAlloc.h"
#include "aostl/uvector.h"

class FramePool {
public:
  FramePool(size_t count, uint32 page_size = PAGE_SIZE);
  ~FramePool();

  uint32 pop();

  bool empty() const { return pages_.empty(); }
  size_t size() const { return pages_.size(); }

  void freeAll();

private:
  aostl::vector<uint32> pages_;
  uint32 page_size_;
};
