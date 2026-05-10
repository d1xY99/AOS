#include "kmalloc.h"
#include "KernelHeap.h"
#include "backtrace.h"

void* kmalloc(size_t size)
{
  pointer called_by = getCalledBefore(1);
  return (void*)KernelHeap::instance()->allocateMemory(size, called_by);
}

void kfree(void * address)
{
  KernelHeap::instance()->freeMemory((pointer)address, getCalledBefore(1));
}

void* krealloc(void * address, size_t size)
{
  return (void*) KernelHeap::instance()->reallocateMemory((pointer)address, size, getCalledBefore(1));
}
