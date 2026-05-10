#pragma once

#include "FileDescriptor.h"
#include "FileSlot.h"
#include "Mutex.h"
#include "paging-definitions.h"
#include "kernel_user_shared.h"
#include "types.h"
#include "ulist.h"
#include "umap.h"
#include "aostl/ustring.h"
#include "uvector.h"

class ArchMemory;
class UserProcess;
class Inode;

class ShMemSegment : public FileDescriptor
{
public:
  ShMemSegment();
  explicit ShMemSegment(FileDescriptor *file_fd);
  explicit ShMemSegment(const aostl::string &name);
  ~ShMemSegment();

  void ensureSize(size_t min_size);
  void setSize(size_t new_size);
  bool getPageState(size_t page_index, size_t &ppn, size_t &swap_slot,
                    bool &swapped, bool &allocated);
  size_t ensureResidentPage(size_t page_index);
  void markPageSwapped(size_t page_index, size_t swap_slot);
  void markPageResident(size_t page_index, size_t ppn);
  bool writeBackPage(size_t page_index, const void *page_ptr);

  size_t size() const { return size_; }
  const aostl::string &getName() const { return name_; }
  bool isFileBacked() const { return file_backed_; }
  Inode *backingInode() const;

private:
  struct PageState
  {
    size_t ppn = 0;
    size_t swap_slot = 0;
    bool swapped = false;
    bool allocated = false;
  };

  aostl::vector<PageState> pages_;
  size_t size_ = 0;
  aostl::string name_;
  FileDescriptor *file_fd_ = nullptr;
  bool file_backed_ = false;
  Mutex lock_;
};

struct ShMemBinding
{
  ShMemBinding(size_t start_vpn, size_t num_pages, int prot, int flags,
                 off_t offset, FileDescriptor *fd, bool add_ref = true,
                 bool backing_is_shared_object = false);
  ~ShMemBinding();

  bool containsVPN(size_t vpn) const
  {
    return vpn >= start_vpn_ && vpn < start_vpn_ + num_pages_;
  }

  bool isShared() const { return (flags_ & MAP_SHARED) != 0; }
  bool isAnonymous() const { return (flags_ & MAP_ANONYMOUS) != 0; }
  bool hasFD() const { return backing_fd_ != nullptr; }
  size_t endVPN() const { return start_vpn_ + num_pages_; }
  size_t pageIndexForVPN(size_t vpn) const
  {
    return (offset_ / PAGE_SIZE) + (vpn - start_vpn_);
  }

  size_t start_vpn_;
  size_t num_pages_;
  int prot_;
  int flags_;
  off_t offset_;
  FileDescriptor *backing_fd_;
  bool owns_fd_;
  bool backing_is_shared_object_;
};

class ShMemRegistry
{
public:
  ShMemRegistry(UserProcess *process, ArchMemory *arch_memory);
  ShMemRegistry(const ShMemRegistry &other, UserProcess *process,
                   ArchMemory *arch_memory);
  ~ShMemRegistry();

  void *mmap(void *start, size_t length, int prot, int flags, int fd,
             off_t offset);
  int munmap(void *start, size_t length);
  int mprotect(void *start, size_t length, int prot);
  bool handleSharedPageFault(size_t address, bool writing);
  void unmapAllPages(ArchMemory *arch_memory);

  bool handlePageSwappedOut(size_t vpn, size_t swap_slot);
  bool handlePageSwappedIn(size_t vpn, size_t ppn);

  static int shm_open(const char *name, int oflag, mode_t mode,
                      UserProcess *process);
  static int shm_unlink(const char *name);

  Mutex shared_mem_lock_;

private:
  size_t createEntry(size_t start_vpn, size_t num_pages, int prot, int flags,
                     int fd, off_t offset);
  ShMemBinding *entryForVPN(size_t vpn);
  aostl::vector<size_t> collectPagesInRange(size_t start_vpn, size_t num_pages);
  void unmapSinglePage(ShMemBinding *entry, size_t vpn);
  void unmapPageForEntry(ShMemBinding *entry, size_t vpn);
  void applyProtectionRange(size_t start_vpn, size_t num_pages, int prot);

  bool mapSharedPage(size_t vpn, size_t ppn, bool writeable, bool executable,
                     bool shared);
  bool mapSwappedOutPage(size_t vpn, size_t swap_slot, bool writeable,
                         bool executable, bool shared);
  void updatePteFlags(size_t vpn, bool writeable, bool executable, bool shared);

  bool rangeOverlaps(size_t start_vpn, size_t num_pages) const;
  size_t findFreeRange(size_t hint_vpn, size_t num_pages);

  UserProcess *process_;
  ArchMemory *arch_memory_;
  aostl::list<ShMemBinding *> entries_;
  size_t last_free_vpn_;

  static aostl::map<aostl::string, ShMemSegment *> &shmObjects();
  static Mutex &shmLock();
};
