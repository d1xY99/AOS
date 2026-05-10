#pragma once

#include "Mutex.h"
#include "types.h"
#include "umap.h"
#include "ustring.h"

struct HeapPageInfo {
  bool allocated;
};

class UserProcess;

class HeapIface {
public:
  enum HEAP_REGION { IN_HEAP, OUT_OF_HEAP };

  HeapIface(UserProcess *process);
  HeapIface(HeapIface *other);
  ~HeapIface();

  size_t first_vpn_;
  void *heap_end_address_;
  bool heap_initialized_;

  bool setHeapEnd(void *end_address);

  void *incHeapBy(size_t size);

  void *getHeapEndVirtualAddress();
  void *getHeapStartVirtualAddress();

  HEAP_REGION getHeapRegion(size_t address);

  aostl::string getHeapRegionPrintable(HEAP_REGION region);

  bool allocateHeapPage(size_t address);

  UserProcess *process_;

  void addAllocatedPage(size_t vpn, HeapPageInfo *info);

  aostl::map<size_t, HeapPageInfo *> allocated_pages_;

  void resetHeapStacks();

private:
  Mutex lock_;
};
