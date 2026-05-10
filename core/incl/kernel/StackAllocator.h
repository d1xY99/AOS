#pragma once

#include "Loader.h"
#include "Mutex.h"
#include "kernel_user_shared.h"
#include "types.h"
#include "umap.h"
#include "uvector.h"

class UserProcess;
class UserThread;

struct StackDescriptor {
  void *stack_start;
  void *pthread_info_start;
  void *tls_data_start;
  size_t tls_data_size;
  size_t tls_allocation_size;
  aostl::vector<size_t> allocated_vpns;
  bool allocated;
};

class StackAllocator {

public:
  enum STACK_REGION {
    STACK_GUARD_PAGE,
    EXEC_POINTER_PAGE,
    EXEC_PARAM_PAGE,
    OWN_THREAD_STACK,
    OTHER_THREAD_STACK,
    OUT_OF_STACK_REGION,
    NOT_DEFINED
  };

  StackAllocator(UserProcess *process);
  StackAllocator(StackAllocator *other);
  virtual ~StackAllocator();

  Mutex stack_info_lock_;
  aostl::map<size_t, StackDescriptor *> stack_infos_;

  size_t first_vpn_;

  size_t exec_param_page_vpn_;
  size_t exec_param_pointer_page_vpn_;
  size_t exec_param_page_ppn_;
  size_t exec_param_pointer_page_ppn_;
  size_t argv_count_ = 0;
  bool exec_param_pointer_page_allocated_ = false;
  bool exec_param_page_allocated_ = false;

  // Stack grows downwards, so highest address is the start of the page
  size_t vpnToPageStartAddress(size_t vpn);

  size_t vpnToPageEndAddress(size_t vpn);

  UserProcess *process_;

  STACK_REGION getStackRegion(size_t address, UserThread *thread);

  void setupThreadMemoryRegion(UserThread *thread);

  void *setupThreadStack(UserThread *thread);

  bool allocateStackForThread(size_t address);

  const char *getStackRegionString(STACK_REGION region);

  StackDescriptor *getStackInfoForThread(size_t thread_id);

  void setStackInfoForThread(size_t thread_id, StackDescriptor *info);

  void unmapStackForThread(UserThread *thread);

  void resetThreadStacks();

  bool writeExecParamPage(char *const argv[]);

  void mapParamPages(Loader *loader);

  char **readExecParamPage();

  size_t getArgvCount();

  StackDescriptor *cloneStackInfoFrom(StackAllocator *other, size_t thread_id);

  size_t getExecParamPointerPageVPN();

  size_t getExecParamPageVPN();

  size_t getStackRegionStartVPN(size_t thread_id);

  size_t getNextGuardPageVPN(size_t thread_id);

  size_t getStackStartVPN(size_t thread_id);

  size_t addressToVPN(size_t address);

  size_t vpnToThreadID(size_t vpn);
};
