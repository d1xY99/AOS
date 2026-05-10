#include "ShMemRegistry.h"

#include "ArchMemory.h"
#include "InvPageTable.h"
#include "FileSlotTable.h"
#include "FrameAlloc.h"
#include "Scheduler.h"
#include "ScopeLock.h"
#include "SpinLock.h"
#include "SwapEngine.h"
#include "UserProcess.h"
#include "Inode.h"
#include "assert.h"
#include "debug.h"
#include "File.h"
#include "kstring.h"
#include "new.h"

namespace
{
constexpr int VALID_FLAG_SET_1 = MAP_PRIVATE;
constexpr int VALID_FLAG_SET_2 = MAP_SHARED;
constexpr int VALID_FLAG_SET_3 = MAP_PRIVATE | MAP_ANONYMOUS;
constexpr int VALID_FLAG_SET_4 = MAP_SHARED | MAP_ANONYMOUS;
constexpr size_t SHM_NAME_MAX = 255;

bool isValidSharedMemName(const char *name)
{
  size_t index = 0;
  while (index <= SHM_NAME_MAX)
  {
    if (name[index] == '\0')
    {
      return true;
    }
    ++index;
  }
  return false;
}
}

namespace
{
alignas(aostl::map<aostl::string, ShMemSegment *>)
    static unsigned char
        shm_objects_storage[sizeof(aostl::map<aostl::string, ShMemSegment *>)];
alignas(Mutex) static unsigned char shm_lock_storage[sizeof(Mutex)];
static aostl::map<aostl::string, ShMemSegment *> *shm_objects_ptr = nullptr;
static Mutex *shm_lock_ptr = nullptr;
static bool shm_initialized = false;
alignas(SpinLock) static unsigned char shm_init_lock_storage[sizeof(SpinLock)];
static SpinLock *shm_init_lock_ptr = nullptr;

void ensureSharedStatics()
{
  if (shm_initialized)
  {
    return;
  }

  if (!shm_init_lock_ptr)
  {
    shm_init_lock_ptr =
        new (shm_init_lock_storage) SpinLock("ShMemRegistry::init_lock");
  }
  shm_init_lock_ptr->acquire();
  if (!shm_initialized)
  {
    shm_objects_ptr =
        new (shm_objects_storage) aostl::map<aostl::string, ShMemSegment *>();
    shm_lock_ptr = new (shm_lock_storage) Mutex("ShMemRegistry::shm_lock_");
    shm_initialized = true;
  }
  shm_init_lock_ptr->release();
}
} // namespace

aostl::map<aostl::string, ShMemSegment *> &ShMemRegistry::shmObjects()
{
  ensureSharedStatics();
  return *shm_objects_ptr;
}

Mutex &ShMemRegistry::shmLock()
{
  ensureSharedStatics();
  return *shm_lock_ptr;
}

ShMemSegment::ShMemSegment()
    : FileDescriptor(nullptr), lock_("ShMemSegment::lock_")
{
}

ShMemSegment::ShMemSegment(FileDescriptor *file_fd)
    : FileDescriptor(nullptr), file_fd_(file_fd),
      file_backed_(file_fd != nullptr),
      lock_("ShMemSegment::lock_")
{
  if (file_fd_)
  {
    file_fd_->incref();
  }
}

ShMemSegment::ShMemSegment(const aostl::string &name)
    : FileDescriptor(nullptr), name_(name), lock_("ShMemSegment::lock_")
{
}

ShMemSegment::~ShMemSegment()
{
  ScopeLock lock(lock_);
  for (auto &page : pages_)
  {
    if (page.ppn != 0 && !page.swapped)
    {
      FrameAlloc::instance()->freePPN(page.ppn);
    }
  }
  pages_.clear();
  size_ = 0;
  if (file_fd_)
  {
    bool last = file_fd_->decref();
    if (last)
    {
      assert(!global_fd_list.remove(file_fd_) &&
             "ShMemSegment::~ShMemSegment: Failed to remove backing fd from global fd list");
      if (File *file = file_fd_->getFile())
      {
        file->closeFd(file_fd_);
      }
      else
      {
        delete file_fd_;
      }
    }
    file_fd_ = nullptr;
  }
}

void ShMemSegment::ensureSize(size_t min_size)
{
  ScopeLock lock(lock_);
  if (size_ < min_size)
  {
    size_t needed_pages = (min_size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages_.size() < needed_pages)
    {
      pages_.resize(needed_pages);
    }
    size_ = min_size;
  }
}

void ShMemSegment::setSize(size_t new_size)
{
  ScopeLock lock(lock_);
  if (new_size > size_)
  {
    size_t needed_pages = (new_size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages_.size() < needed_pages)
    {
      pages_.resize(needed_pages);
    }
  }
  size_ = new_size;
}

bool ShMemSegment::getPageState(size_t page_index, size_t &ppn,
                                   size_t &swap_slot, bool &swapped,
                                   bool &allocated)
{
  ScopeLock lock(lock_);
  if (page_index >= pages_.size())
  {
    ppn = 0;
    swap_slot = 0;
    swapped = false;
    allocated = false;
    return false;
  }
  const PageState &state = pages_[page_index];
  ppn = state.ppn;
  swap_slot = state.swap_slot;
  swapped = state.swapped;
  allocated = state.allocated;
  return true;
}

size_t ShMemSegment::ensureResidentPage(size_t page_index)
{
  bool needs_allocation = false;
  {
    ScopeLock lock(lock_);
    if (page_index >= pages_.size())
    {
      pages_.resize(page_index + 1);
    }
    PageState &state = pages_[page_index];
    if (state.swapped)
    {
      return 0;
    }
    if (state.allocated)
    {
      return state.ppn;
    }
    needs_allocation = true;
  }

  size_t ppn = 0;
  if (needs_allocation)
  {
    ppn = FrameAlloc::instance()->allocPPN();
    void *page_ptr = (void *)ArchMemory::getIdentAddressOfPPN(ppn);
    memset(page_ptr, 0, PAGE_SIZE);
    if (file_backed_ && file_fd_ && file_fd_->getFile())
    {
      off_t file_offset = static_cast<off_t>(page_index) * PAGE_SIZE;
      File *file = file_fd_->getFile();
      Inode *inode = nullptr;
      if (file)
      {
        inode = file->getInode();
      }

      if (inode)
      {
        int32 read_bytes =
            inode->readData(file_offset, PAGE_SIZE, static_cast<char *>(page_ptr));
        if (read_bytes < 0)
        {
          memset(page_ptr, 0, PAGE_SIZE);
        }
      }
      else
      {
        memset(page_ptr, 0, PAGE_SIZE);
      }
    }
  }

  ScopeLock lock(lock_);
  if (page_index >= pages_.size())
  {
    pages_.resize(page_index + 1);
  }
  PageState &state = pages_[page_index];
  if (state.swapped)
  {
    if (ppn != 0)
    {
      FrameAlloc::instance()->freePPN(ppn);
    }
    return 0;
  }
  if (state.allocated)
  {
    if (ppn != 0)
    {
      FrameAlloc::instance()->freePPN(ppn);
    }
    return state.ppn;
  }
  state.ppn = ppn;
  state.swap_slot = 0;
  state.swapped = false;
  state.allocated = true;
  return state.ppn;
}

void ShMemSegment::markPageSwapped(size_t page_index, size_t swap_slot)
{
  ScopeLock lock(lock_);
  if (page_index >= pages_.size())
  {
    pages_.resize(page_index + 1);
  }
  PageState &state = pages_[page_index];
  state.ppn = 0;
  state.swap_slot = swap_slot;
  state.swapped = true;
  state.allocated = true;
}

void ShMemSegment::markPageResident(size_t page_index, size_t ppn)
{
  ScopeLock lock(lock_);
  if (page_index >= pages_.size())
  {
    pages_.resize(page_index + 1);
  }
  PageState &state = pages_[page_index];
  state.ppn = ppn;
  state.swap_slot = 0;
  state.swapped = false;
  state.allocated = true;
}

bool ShMemSegment::writeBackPage(size_t page_index, const void *page_ptr)
{
  ScopeLock lock(lock_);
  if (!page_ptr || !file_backed_ || !file_fd_ || !file_fd_->getFile())
  {
    return false;
  }

  File *file = file_fd_->getFile();
  if ((file->getFlag() & (O_WRONLY | O_RDWR)) == 0)
  {
    return false;
  }

  off_t file_offset = static_cast<off_t>(page_index) * PAGE_SIZE;
  Inode *inode = file->getInode();

  if (!inode)
  {
    return false;
  }

  int32 written = inode->writeData(file_offset, PAGE_SIZE,
                      static_cast<const char *>(page_ptr));
  return written == static_cast<int32>(PAGE_SIZE);
}

Inode *ShMemSegment::backingInode() const
{
  if (!file_fd_)
    return nullptr;

  File *file = file_fd_->getFile();
  Inode *inode = nullptr;

  if (file)
    inode = file->getInode();

  return inode;
}

ShMemBinding::ShMemBinding(size_t start_vpn, size_t num_pages, int prot,
                               int flags, off_t offset, FileDescriptor *fd,
                               bool add_ref, bool backing_is_shared_object)
    : start_vpn_(start_vpn), num_pages_(num_pages), prot_(prot), flags_(flags),
      offset_(offset), backing_fd_(fd), owns_fd_(add_ref),
      backing_is_shared_object_(backing_is_shared_object)
{
  if (backing_fd_ && add_ref)
  {
    backing_fd_->incref();
  }
}

ShMemBinding::~ShMemBinding()
{
  if (backing_fd_ && owns_fd_)
  {
    bool last = backing_fd_->decref();
    if (last)
    {
      assert(!global_fd_list.remove(backing_fd_) &&
             "ShMemBinding::~ShMemBinding: Failed to remove backing fd from global fd list");
      if (backing_fd_->getFile())
      {
        backing_fd_->getFile()->closeFd(backing_fd_);
      }
      else
      {
        delete backing_fd_;
      }
    }
    backing_fd_ = nullptr;
  }
}

ShMemRegistry::ShMemRegistry(UserProcess *process, ArchMemory *arch_memory)
    : shared_mem_lock_("ShMemRegistry::shared_mem_lock_"), process_(process),
      arch_memory_(arch_memory), entries_(), last_free_vpn_(MIN_SHARED_MEM_VPN)
{
}

ShMemRegistry::ShMemRegistry(const ShMemRegistry &other,
                                   UserProcess *process,
                                   ArchMemory *arch_memory)
    : shared_mem_lock_("ShMemRegistry::shared_mem_lock_"), process_(process),
      arch_memory_(arch_memory), entries_(),
      last_free_vpn_(other.last_free_vpn_)
{
  for (auto *entry : other.entries_)
  {
    ShMemBinding *clone = new ShMemBinding(
        entry->start_vpn_, entry->num_pages_, entry->prot_, entry->flags_,
        entry->offset_, entry->backing_fd_, true, entry->backing_is_shared_object_);
    entries_.push_back(clone);
  }
}

ShMemRegistry::~ShMemRegistry()
{
  unmapAllPages(arch_memory_);
}

void *ShMemRegistry::mmap(void *start, size_t length, int prot, int flags,
                             int fd, off_t offset)
{
  if (length == 0)
  {
    return MAP_FAILED;
  }

  const int allowed_prot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_NONE;
  if (prot & ~allowed_prot)
  {
    return MAP_FAILED;
  }

  if ((prot & PROT_WRITE) && !(prot & PROT_READ))
  {
    return MAP_FAILED;
  }

  if (offset % PAGE_SIZE != 0)
  {
    return MAP_FAILED;
  }

  if (start && ((size_t)start % PAGE_SIZE))
  {
    return MAP_FAILED;
  }

  if (flags != VALID_FLAG_SET_1 && flags != VALID_FLAG_SET_2 &&
      flags != VALID_FLAG_SET_3 && flags != VALID_FLAG_SET_4)
  {
    return MAP_FAILED;
  }

  if (!(flags & MAP_ANONYMOUS) && !(flags & MAP_SHARED) &&
      !(flags & MAP_PRIVATE))
  {
    return MAP_FAILED;
  }

  if (!(flags & MAP_ANONYMOUS) && fd < 0)
  {
    return MAP_FAILED;
  }

  size_t num_pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;

  shared_mem_lock_.acquire();
  size_t start_vpn = 0;
  if (start)
  {
    start_vpn = (size_t)start / PAGE_SIZE;
    if (start_vpn < MIN_SHARED_MEM_VPN ||
        start_vpn + num_pages > MAX_SHARED_MEM_VPN)
    {
      shared_mem_lock_.release();
      return MAP_FAILED;
    }
  }

  size_t mapped_vpn = findFreeRange(start_vpn, num_pages);
  if (mapped_vpn == 0)
  {
    shared_mem_lock_.release();
    return MAP_FAILED;
  }

  mapped_vpn = createEntry(mapped_vpn, num_pages, prot, flags, fd, offset);
  shared_mem_lock_.release();

  if (mapped_vpn == 0)
  {
    return MAP_FAILED;
  }
  return (void *)(mapped_vpn * PAGE_SIZE);
}

size_t ShMemRegistry::createEntry(size_t start_vpn, size_t num_pages,
                                     int prot, int flags, int fd, off_t offset)
{
  FileDescriptor *backing_fd = nullptr;
  bool backing_is_shared_object = false;

  if (flags & (MAP_SHARED | MAP_PRIVATE))
  {
    if (!(flags & MAP_ANONYMOUS))
    {
      FileSlot *local_fd =
          process_->fileDescriptorTable().getLocalDescriptor(fd);
      if (!local_fd)
      {
        return 0;
      }
      if (local_fd->getType() ==
          FileSlot::FileType::SHARED_MEMORY)
      {
        backing_is_shared_object = true;
        backing_fd = local_fd->getKernelDescriptor();
      }
      else if (local_fd->getType() ==
               FileSlot::FileType::NORMAL)
      {
        FileDescriptor *file_fd = local_fd->getKernelDescriptor();
        if (!file_fd)
        {
          return 0;
        }
        ShMemSegment *obj = nullptr;
        if (flags & MAP_SHARED)
        {
          File *file = file_fd->getFile();
          Inode *inode = nullptr;
          if (file)
            inode = file->getInode();

          if (inode)
          {
            for (auto *entry : entries_)
            {
              if (!entry->backing_is_shared_object_ || !entry->isShared() ||
                  !entry->backing_fd_)
              {
                continue;
              }
              ShMemSegment *existing =
                  static_cast<ShMemSegment *>(entry->backing_fd_);
              if (existing->isFileBacked() &&
                  existing->backingInode() == inode)
              {
                obj = existing;
                break;
              }
            }
          }
        }
        if (!obj)
        {
          obj = new ShMemSegment(file_fd);
          assert(!global_fd_list.add(obj));
        }
        backing_is_shared_object = true;
        backing_fd = obj;
      }
      else
      {
        return 0;
      }
    }
    else
    {
      if (flags & MAP_SHARED)
      {
        ShMemSegment *obj = new ShMemSegment();
        assert(!global_fd_list.add(obj));
        backing_is_shared_object = true;
        backing_fd = obj;
      }
    }
  }

  ShMemBinding *entry =
      new ShMemBinding(start_vpn, num_pages, prot, flags, offset, backing_fd,
                         true, backing_is_shared_object);
  if (backing_is_shared_object && backing_fd)
  {
    ShMemSegment *obj = static_cast<ShMemSegment *>(backing_fd);
    obj->ensureSize(offset + num_pages * PAGE_SIZE);
  }
  entries_.push_back(entry);

  size_t next_free = start_vpn + num_pages;
  if (next_free > last_free_vpn_)
  {
    last_free_vpn_ = next_free;
  }
  return start_vpn;
}

bool ShMemRegistry::rangeOverlaps(size_t start_vpn, size_t num_pages) const
{
  size_t end_vpn = start_vpn + num_pages;
  for (auto *entry : entries_)
  {
    if (start_vpn < entry->endVPN() && end_vpn > entry->start_vpn_)
    {
      return true;
    }
  }
  return false;
}

size_t ShMemRegistry::findFreeRange(size_t hint_vpn, size_t num_pages)
{
  if (num_pages == 0)
  {
    return 0;
  }
  size_t max_range = MAX_SHARED_MEM_VPN - MIN_SHARED_MEM_VPN;
  if (num_pages > max_range)
  {
    return 0;
  }

  size_t max_start = MAX_SHARED_MEM_VPN - num_pages;
  if (hint_vpn != 0)
  {
    if (hint_vpn >= MIN_SHARED_MEM_VPN && hint_vpn <= max_start &&
        !rangeOverlaps(hint_vpn, num_pages))
    {
      return hint_vpn;
    }
  }

  if (SHM_ASLR)
  {
    size_t min_start = MIN_SHARED_MEM_VPN;
    size_t max_start_inclusive = max_start;
    if (max_start_inclusive > min_start)
    {
      for (size_t attempt = 0; attempt < SHM_ASLR_ATTEMPTS; ++attempt)
      {
        size_t candidate = Scheduler::instance()->getRandomNumberInRange(
            min_start, max_start_inclusive + 1);
        if (!rangeOverlaps(candidate, num_pages))
        {
          return candidate;
        }
      }
    }
  }

  size_t candidate = last_free_vpn_;
  if (candidate < MIN_SHARED_MEM_VPN)
  {
    candidate = MIN_SHARED_MEM_VPN;
  }
  if (candidate <= max_start && !rangeOverlaps(candidate, num_pages))
  {
    return candidate;
  }

  size_t vpn = MIN_SHARED_MEM_VPN;
  while (vpn <= max_start)
  {
    if (!rangeOverlaps(vpn, num_pages))
    {
      return vpn;
    }

    size_t next = vpn + 1;
    for (auto *entry : entries_)
    {
      if (vpn >= entry->start_vpn_ && vpn < entry->endVPN())
      {
        next = entry->endVPN();
        break;
      }
    }
    vpn = next;
  }
  return 0;
}

int ShMemRegistry::munmap(void *start, size_t length)
{
  if (!start || length == 0)
  {
    return -1;
  }
  if ((size_t)start % PAGE_SIZE != 0)
  {
    return -1;
  }

  size_t start_vpn = (size_t)start / PAGE_SIZE;
  size_t num_pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;

  if (start_vpn < MIN_SHARED_MEM_VPN ||
      start_vpn + num_pages > MAX_SHARED_MEM_VPN)
  {
    return -1;
  }

  shared_mem_lock_.acquire();
  aostl::vector<size_t> vpns = collectPagesInRange(start_vpn, num_pages);
  if (vpns.size() < num_pages)
  {
    shared_mem_lock_.release();
    return -1;
  }
  for (size_t vpn : vpns)
  {
    ShMemBinding *entry = entryForVPN(vpn);
    if (entry)
    {
      unmapSinglePage(entry, vpn);
    }
  }
  shared_mem_lock_.release();
  return 0;
}

void ShMemRegistry::applyProtectionRange(size_t start_vpn, size_t num_pages,
                                            int prot)
{
  size_t end_vpn = start_vpn + num_pages;
  aostl::vector<ShMemBinding *> to_remove;
  aostl::vector<ShMemBinding *> to_add;

  for (auto *entry : entries_)
  {
    size_t entry_start = entry->start_vpn_;
    size_t entry_end = entry->endVPN();
    if (entry_end <= start_vpn || entry_start >= end_vpn)
      continue;

    size_t overlap_start =
        (entry_start > start_vpn) ? entry_start : start_vpn;
    size_t overlap_end = (entry_end < end_vpn) ? entry_end : end_vpn;

    if (overlap_start == entry_start && overlap_end == entry_end)
    {
      entry->prot_ = prot;
      continue;
    }

    if (overlap_start > entry_start)
    {
      size_t left_pages = overlap_start - entry_start;
      ShMemBinding *left = new ShMemBinding(
          entry_start, left_pages, entry->prot_, entry->flags_, entry->offset_,
          entry->backing_fd_, true, entry->backing_is_shared_object_);
      to_add.push_back(left);
    }

    size_t mid_pages = overlap_end - overlap_start;
    ShMemBinding *mid = new ShMemBinding(
        overlap_start, mid_pages, prot, entry->flags_,
        entry->offset_ + (overlap_start - entry_start) * PAGE_SIZE,
        entry->backing_fd_, true, entry->backing_is_shared_object_);
    to_add.push_back(mid);

    if (overlap_end < entry_end)
    {
      size_t right_pages = entry_end - overlap_end;
      ShMemBinding *right = new ShMemBinding(
          overlap_end, right_pages, entry->prot_, entry->flags_,
          entry->offset_ + (overlap_end - entry_start) * PAGE_SIZE,
          entry->backing_fd_, true, entry->backing_is_shared_object_);
      to_add.push_back(right);
    }

    to_remove.push_back(entry);
  }

  for (auto *entry : to_remove)
  {
    entries_.remove(entry);
    delete entry;
  }
  for (auto *entry : to_add)
  {
    entries_.push_back(entry);
  }
}

int ShMemRegistry::mprotect(void *start, size_t length, int prot)
{
  if (!start || length == 0)
  {
    return -1;
  }
  if ((size_t)start % PAGE_SIZE != 0)
  {
    return -1;
  }

  const int allowed_prot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_NONE;
  if (prot & ~allowed_prot)
  {
    return -1;
  }

  size_t start_vpn = (size_t)start / PAGE_SIZE;
  size_t num_pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;

  if (start_vpn < MIN_SHARED_MEM_VPN ||
      start_vpn + num_pages > MAX_SHARED_MEM_VPN)
  {
    return -1;
  }

  shared_mem_lock_.acquire();
  aostl::vector<size_t> vpns = collectPagesInRange(start_vpn, num_pages);
  if (vpns.size() < num_pages)
  {
    shared_mem_lock_.release();
    return -1;
  }
  applyProtectionRange(start_vpn, num_pages, prot);
  shared_mem_lock_.release();

  bool want_write = (prot & PROT_WRITE) != 0;
  bool want_exec = (prot & PROT_EXEC) != 0;

  bool locked_before =
      arch_memory_->arch_memory_lock_.heldBy() == currentThread;
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.acquire();
  }
  for (size_t vpn : vpns)
  {
    ArchMemoryMapping mapping = arch_memory_->resolveMapping(vpn);
    if (!mapping.pt)
      continue;
    PageTableEntry &pte = mapping.pt[mapping.pti];
    if (!pte.present && !pte.swapped_out)
      continue;
    pte.writeable = want_write ? 1 : 0;
    pte.execution_disabled = want_exec ? 0 : 1;
  }
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.release();
  }
  ArchMemory::flushTlb();

  return 0;
}

aostl::vector<size_t>
ShMemRegistry::collectPagesInRange(size_t start_vpn, size_t num_pages)
{
  aostl::vector<size_t> vpns;
  size_t end_vpn = start_vpn + num_pages;
  for (auto *entry : entries_)
  {
    size_t entry_end = entry->endVPN();
    if (entry->start_vpn_ >= end_vpn || start_vpn >= entry_end)
    {
      continue;
    }
    size_t begin =
        (start_vpn > entry->start_vpn_) ? start_vpn : entry->start_vpn_;
    size_t end = (end_vpn < entry_end) ? end_vpn : entry_end;
    for (size_t vpn = begin; vpn < end; ++vpn)
      vpns.push_back(vpn);
  }
  return vpns;
}

ShMemBinding *ShMemRegistry::entryForVPN(size_t vpn)
{
  for (auto *entry : entries_)
  {
    if (entry->containsVPN(vpn))
    {
      return entry;
    }
  }
  return nullptr;
}

void ShMemRegistry::updatePteFlags(size_t vpn, bool writeable,
                                      bool executable, bool shared)
{
  bool locked_before =
      arch_memory_->arch_memory_lock_.heldBy() == currentThread;
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.acquire();
  }
  ArchMemoryMapping mapping = arch_memory_->resolveMapping(vpn);
  if (mapping.pt)
  {
    PageTableEntry &pte = mapping.pt[mapping.pti];
    pte.writeable = writeable ? 1 : 0;
    pte.execution_disabled = executable ? 0 : 1;
    pte.shared = shared ? 1 : 0;
    pte.copy_on_write = 0;
  }
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.release();
  }
}

bool ShMemRegistry::mapSharedPage(size_t vpn, size_t ppn, bool writeable,
                                     bool executable, bool shared)
{
  bool locked_before =
      arch_memory_->arch_memory_lock_.heldBy() == currentThread;
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.acquire();
  }
  ArchMemoryMapping mapping = arch_memory_->resolveMapping(vpn);
  if (mapping.pt && mapping.pt[mapping.pti].present)
  {
    if (!locked_before)
    {
      arch_memory_->arch_memory_lock_.release();
    }
    updatePteFlags(vpn, writeable, executable, shared);
    return true;
  }
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.release();
  }

  bool mapped = arch_memory_->mapPage(vpn, ppn, 1, writeable, executable);
  if (!mapped)
  {
    return false;
  }
  updatePteFlags(vpn, writeable, executable, shared);
  return true;
}

bool ShMemRegistry::mapSwappedOutPage(size_t vpn, size_t swap_slot,
                                         bool writeable, bool executable,
                                         bool shared)
{
  size_t temp_ppn = FrameAlloc::instance()->allocPPN();
  bool mapped = arch_memory_->mapPage(vpn, temp_ppn, 1, writeable, executable);
  if (!mapped)
  {
    FrameAlloc::instance()->freePPN(temp_ppn);
    return false;
  }

  InvPageTable::instance()->lock_.acquire();
  arch_memory_->arch_memory_lock_.acquire();
  ArchMemoryMapping mapping = arch_memory_->resolveMapping(vpn);
  InvPageTable::instance()->findAndRemoveMappingFromIPTEntry(
      &mapping.pt[mapping.pti], arch_memory_, vpn);
  arch_memory_->arch_memory_lock_.release();
  InvPageTable::instance()->lock_.release();

  arch_memory_->arch_memory_lock_.acquire();
  mapping = arch_memory_->resolveMapping(vpn);
  SwapEngine::instance()->changePTEToSwappedOut(mapping.pt[mapping.pti],
                                                 swap_slot);
  mapping.pt[mapping.pti].writeable = writeable ? 1 : 0;
  mapping.pt[mapping.pti].execution_disabled = executable ? 0 : 1;
  mapping.pt[mapping.pti].shared = shared ? 1 : 0;
  mapping.pt[mapping.pti].copy_on_write = 0;
  arch_memory_->arch_memory_lock_.release();

  FrameAlloc::instance()->freePPN(temp_ppn);

  InvPageTable::instance()->lock_.acquire();
  InvPageTableEntry *entry =
      SwapEngine::instance()->getSwappedOutIptEntry(swap_slot);
  entry->insertMappingToEntry(process_, vpn);
  InvPageTable::instance()->lock_.release();

  return true;
}

bool ShMemRegistry::handleSharedPageFault(size_t address, bool writing)
{
  size_t vpn = address / PAGE_SIZE;
  ShMemBinding *entry = nullptr;

  shared_mem_lock_.acquire();
  entry = entryForVPN(vpn);
  shared_mem_lock_.release();

  if (!entry)
  {
    return false;
  }

  bool allow_write = (entry->prot_ & PROT_WRITE) != 0;
  bool allow_read = (entry->prot_ & PROT_READ) != 0;

  if (writing && !allow_write)
  {
    return false;
  }

  if (!writing && !allow_read)
  {
    return false;
  }

  bool locked_before =
      arch_memory_->arch_memory_lock_.heldBy() == currentThread;
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.acquire();
  }
  ArchMemoryMapping mapping = arch_memory_->resolveMapping(vpn);
  bool present = mapping.pt && mapping.pt[mapping.pti].present;
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.release();
  }
  if (present)
  {
    updatePteFlags(vpn, (entry->prot_ & PROT_WRITE) != 0,
                   (entry->prot_ & PROT_EXEC) != 0, entry->isShared());
    return true;
  }

  bool writeable = (entry->prot_ & PROT_WRITE) != 0;
  bool executable = (entry->prot_ & PROT_EXEC) != 0;

  if (entry->backing_is_shared_object_ && entry->backing_fd_)
  {
    ShMemSegment *obj = static_cast<ShMemSegment *>(entry->backing_fd_);
    bool is_shared = entry->isShared();
    size_t page_index = entry->pageIndexForVPN(vpn);
    size_t ppn = 0;
    size_t swap_slot = 0;
    bool swapped = false;
    bool allocated = false;
    obj->getPageState(page_index, ppn, swap_slot, swapped, allocated);
    if (swapped)
    {
      return mapSwappedOutPage(vpn, swap_slot, writeable, executable,
                               is_shared);
    }

    if (!allocated)
    {
      ppn = obj->ensureResidentPage(page_index);
    }

    if (ppn == 0)
    {
      return false;
    }

    FrameAlloc::instance()->increfPPN(static_cast<uint32>(ppn));
    if (!mapSharedPage(vpn, ppn, writeable, executable, is_shared))
    {
      FrameAlloc::instance()->freePPN(static_cast<uint32>(ppn));
      return false;
    }
    return true;
  }

  size_t ppn = FrameAlloc::instance()->allocPPN();
  bool mapped = arch_memory_->mapPage(vpn, ppn, 1, writeable, executable);
  if (!mapped)
  {
    FrameAlloc::instance()->freePPN(static_cast<uint32>(ppn));
    return false;
  }
  updatePteFlags(vpn, writeable, executable, false);
  return true;
}

void ShMemRegistry::unmapSinglePage(ShMemBinding *entry, size_t vpn)
{
  if (!entry)
  {
    return;
  }

  bool locked_before =
      arch_memory_->arch_memory_lock_.heldBy() == currentThread;
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.acquire();
  }

  ArchMemoryMapping mapping = arch_memory_->resolveMapping(vpn);
  bool present = mapping.pt && mapping.pt[mapping.pti].present;
  bool swapped = mapping.pt && mapping.pt[mapping.pti].swapped_out;

  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.release();
  }

  if (present)
  {
    if (entry->isShared() && entry->backing_is_shared_object_ &&
        entry->backing_fd_)
    {
      ShMemSegment *obj = static_cast<ShMemSegment *>(entry->backing_fd_);
      if (obj->isFileBacked())
      {
        size_t page_index = entry->pageIndexForVPN(vpn);
        obj->writeBackPage(page_index,
                           reinterpret_cast<const void *>(mapping.page));
      }
    }
    arch_memory_->unmapPage(vpn);
    ArchMemory::flushTlb();
  }
  else if (swapped)
  {
    arch_memory_->arch_memory_lock_.acquire();
    InvPageTable::instance()->lock_.acquire();
    InvPageTable::instance()->findAndRemoveMappingFromIPTEntry(
        &mapping.pt[mapping.pti], arch_memory_, vpn);
    InvPageTable::instance()->lock_.release();
    ((uint64 *)mapping.pt)[mapping.pti] = 0;
    arch_memory_->arch_memory_lock_.release();
  }

  size_t offset = vpn - entry->start_vpn_;
  if (entry->num_pages_ == 1)
  {
    entries_.remove(entry);
    delete entry;
    return;
  }

  if (offset == 0)
  {
    entry->start_vpn_++;
    entry->num_pages_--;
    entry->offset_ += PAGE_SIZE;
  }
  else if (offset == entry->num_pages_ - 1)
  {
    entry->num_pages_--;
  }
  else
  {
    size_t right_pages = entry->num_pages_ - offset - 1;
    ShMemBinding *split = new ShMemBinding(
        entry->start_vpn_ + offset + 1, right_pages, entry->prot_,
        entry->flags_, entry->offset_ + (offset + 1) * PAGE_SIZE,
        entry->backing_fd_, true, entry->backing_is_shared_object_);
    entries_.push_back(split);
    entry->num_pages_ = offset;
  }
}

void ShMemRegistry::unmapPageForEntry(ShMemBinding *entry, size_t vpn)
{
  if (!entry)
  {
    return;
  }

  bool locked_before =
      arch_memory_->arch_memory_lock_.heldBy() == currentThread;
  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.acquire();
  }

  ArchMemoryMapping mapping = arch_memory_->resolveMapping(vpn);
  bool present = mapping.pt && mapping.pt[mapping.pti].present;
  bool swapped = mapping.pt && mapping.pt[mapping.pti].swapped_out;

  if (!locked_before)
  {
    arch_memory_->arch_memory_lock_.release();
  }

  if (present)
  {
    if (entry->isShared() && entry->backing_is_shared_object_ &&
        entry->backing_fd_)
    {
      ShMemSegment *obj = static_cast<ShMemSegment *>(entry->backing_fd_);
      if (obj->isFileBacked())
      {
        size_t page_index = entry->pageIndexForVPN(vpn);
        obj->writeBackPage(page_index,
                           reinterpret_cast<const void *>(mapping.page));
      }
    }
    arch_memory_->unmapPage(vpn);
    ArchMemory::flushTlb();
  }
  else if (swapped)
  {
    arch_memory_->arch_memory_lock_.acquire();
    InvPageTable::instance()->lock_.acquire();
    InvPageTable::instance()->findAndRemoveMappingFromIPTEntry(
        &mapping.pt[mapping.pti], arch_memory_, vpn);
    InvPageTable::instance()->lock_.release();
    ((uint64 *)mapping.pt)[mapping.pti] = 0;
    arch_memory_->arch_memory_lock_.release();
  }
}

void ShMemRegistry::unmapAllPages(ArchMemory *arch_memory)
{
  if (!arch_memory)
  {
    return;
  }

  shared_mem_lock_.acquire();
  while (!entries_.empty())
  {
    ShMemBinding *entry = entries_.front();
    entries_.pop_front();
    size_t start = entry->start_vpn_;
    size_t pages = entry->num_pages_;
    for (size_t i = 0; i < pages; ++i)
    {
      unmapPageForEntry(entry, start + i);
    }
    delete entry;
  }
  shared_mem_lock_.release();
}

bool ShMemRegistry::handlePageSwappedOut(size_t vpn, size_t swap_slot)
{
  shared_mem_lock_.acquire();
  ShMemBinding *entry = entryForVPN(vpn);
  shared_mem_lock_.release();

  if (!entry || !entry->backing_is_shared_object_ || !entry->backing_fd_)
  {
    return false;
  }

  ShMemSegment *obj = static_cast<ShMemSegment *>(entry->backing_fd_);
  size_t page_index = entry->pageIndexForVPN(vpn);
  obj->markPageSwapped(page_index, swap_slot);
  return true;
}

bool ShMemRegistry::handlePageSwappedIn(size_t vpn, size_t ppn)
{
  shared_mem_lock_.acquire();
  ShMemBinding *entry = entryForVPN(vpn);
  shared_mem_lock_.release();

  if (!entry || !entry->backing_is_shared_object_ || !entry->backing_fd_)
  {
    return false;
  }

  ShMemSegment *obj = static_cast<ShMemSegment *>(entry->backing_fd_);
  size_t page_index = entry->pageIndexForVPN(vpn);
  obj->markPageResident(page_index, ppn);
  return true;
}

int ShMemRegistry::shm_open(const char *name, int oflag, mode_t,
                               UserProcess *process)
{
  if (!name || !process)
  {
    return -1;
  }

  if (!(oflag & (O_RDONLY | O_WRONLY | O_RDWR)))
  {
    return -1;
  }

  if (!isValidSharedMemName(name))
  {
    return -1;
  }

  aostl::string key(name);
  ShMemSegment *obj = nullptr;
  {
    ScopeLock lock(shmLock());
    auto &objects = shmObjects();
    auto it = objects.find(key);
    if (it == objects.end())
    {
      if (!(oflag & O_CREAT))
      {
        return -1;
      }
      obj = new ShMemSegment(key);
      obj->incref();
      assert(!global_fd_list.add(obj));
      objects.insert(aostl::make_pair(key, obj));
    }
    else
    {
      if (oflag & O_EXCL)
      {
        return -1;
      }
      obj = it->second;
    }
  }

  FileSlotTable &table = process->fileDescriptorTable();
  uint32 mod = (oflag & O_RDWR) ? O_RDWR : O_RDONLY;
  size_t fd =
      table.createLocalDescriptor(obj, FileSlot::FileType::SHARED_MEMORY, mod);
  return static_cast<int>(fd);
}

int ShMemRegistry::shm_unlink(const char *name)
{
  if (!name || !isValidSharedMemName(name))
  {
    return -1;
  }
  aostl::string key(name);
  ScopeLock lock(shmLock());
  auto &objects = shmObjects();
  auto it = objects.find(key);
  if (it == objects.end())
  {
    return -1;
  }

  ShMemSegment *obj = it->second;
  objects.erase(it);
  bool last = obj->decref();
  if (last)
  {
    assert(!global_fd_list.remove(obj) &&
           "ShMemRegistry::shm_unlink: Failed to remove shm object from global fd list");
    delete obj;
  }
  return 0;
}
