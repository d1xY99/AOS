#include "ArchMemory.h"
#include "ArchInterrupts.h"
#include "ArchThreads.h"
#include "InvPageTable.h"
#include "FrameAlloc.h"
#include "ScopeLock.h"
#include "Thread.h"
#include "StackAllocator.h"
#include "UserProcess.h"
#include "UserThread.h"
#include "debug.h"
#include "kprintf.h"
#include "kstring.h"
#include "types.h"

PageMapLevel4Entry kernel_page_map_level_4[PAGE_MAP_LEVEL_4_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));
PageDirPointerTableEntry
    kernel_page_directory_pointer_table[2 * PAGE_DIR_POINTER_TABLE_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));
PageDirEntry kernel_page_directory[2 * PAGE_DIR_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));
PageTableEntry kernel_page_table[8 * PAGE_TABLE_ENTRIES]
    __attribute__((aligned(PAGE_SIZE)));

ArchMemory::ArchMemory()
    : arch_memory_lock_("ArchMemory::arch_memory_lock_"),
      clone_successful_(true) {
  uint64 new_pml4_ppn = FrameAlloc::instance()->allocPPN();
  arch_memory_lock_.acquire();
  page_map_level_4_ = new_pml4_ppn;
  PageMapLevel4Entry *new_pml4 =
      (PageMapLevel4Entry *)getIdentAddressOfPPN(page_map_level_4_);
  memcpy((void *)new_pml4, (void *)kernel_page_map_level_4, PAGE_SIZE);
  memset(new_pml4, 0, PAGE_SIZE / 2); // should be zero, this is just for safety
  arch_memory_lock_.release();
}

ArchMemory::ArchMemory(const ArchMemory &src) : ArchMemory() {
  clone_successful_ = clonePageTables(src);
  if (!clone_successful_) {
    uint64 pml4_to_free = 0;
    arch_memory_lock_.acquire();
    if (page_map_level_4_) {
      pml4_to_free = page_map_level_4_;
      page_map_level_4_ = 0;
    }
    arch_memory_lock_.release();
    if (pml4_to_free)
      FrameAlloc::instance()->freePPN(pml4_to_free);
  }
}

template <typename T>
bool ArchMemory::checkAndRemove(pointer map_ptr, uint64 index) {
  T *map = (T *)map_ptr;
  debug(A_MEMORY, "%s: page %p index %zx\n", __PRETTY_FUNCTION__, map, index);
  ((uint64 *)map)[index] = 0;
  for (uint64 i = 0; i < PAGE_DIR_ENTRIES; i++) {
    if (map[i].present != 0)
      return false;
  }
  return true;
}

bool ArchMemory::unmapPage(uint64 virtual_page) {
  bool locked_before = arch_memory_lock_.heldBy() == currentThread;
  if (!locked_before)
    arch_memory_lock_.acquire();
  assert(arch_memory_lock_.heldBy() == currentThread &&
         "ArchMemory::unmapPage: lock not held by current thread!");

  ArchMemoryMapping m = resolveMapping(virtual_page);

  assert(m.page_ppn != 0 && m.page_size == PAGE_SIZE && m.pt[m.pti].present);
  uint64 to_free[4] = {0, 0, 0, 0};
  size_t free_count = 0;

  InvPageTable::instance()->lock_.acquire();
  InvPageTable::instance()->findAndRemoveMappingFromIPTEntry(
      &m.pt[m.pti], this, virtual_page);
  InvPageTable::instance()->lock_.release();

  m.pt[m.pti].present = 0;
  to_free[free_count++] = m.page_ppn;
  ((uint64 *)m.pt)[m.pti] = 0; // for easier debugging

  bool empty =
      checkAndRemove<PageTableEntry>(getIdentAddressOfPPN(m.pt_ppn), m.pti);

  if (empty) {
    empty = checkAndRemove<PageDirPageTableEntry>(
        getIdentAddressOfPPN(m.pd_ppn), m.pdi);
    to_free[free_count++] = m.pt_ppn;
  }
  if (empty) {
    empty = checkAndRemove<PageDirPointerTablePageDirEntry>(
        getIdentAddressOfPPN(m.pdpt_ppn), m.pdpti);
    to_free[free_count++] = m.pd_ppn;
  }
  if (empty) {
    checkAndRemove<PageMapLevel4Entry>(getIdentAddressOfPPN(m.pml4_ppn),
                                       m.pml4i);
    to_free[free_count++] = m.pdpt_ppn;
  }

  arch_memory_lock_.release();
  for (size_t i = 0; i < free_count; ++i) {
    if (to_free[i])
      FrameAlloc::instance()->freePPN(to_free[i]);
  }
  if (locked_before)
    arch_memory_lock_.acquire();
  return true;
}

// Mutex ArchMemory::clone_lock_("ArchMemory::clone_lock_");

bool ArchMemory::clonePageTables(const ArchMemory &src) {
  arch_memory_lock_.acquire();
  PageMapLevel4Entry *dest_pml4 =
      (PageMapLevel4Entry *)getIdentAddressOfPPN(page_map_level_4_);
  PageMapLevel4Entry *src_pml4 =
      (PageMapLevel4Entry *)getIdentAddressOfPPN(src.page_map_level_4_);

  memset(dest_pml4, 0, PAGE_SIZE / 2);

  size_t required_pages = 0;
  (void)required_pages;
  for (uint64 pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; ++pml4i) {
    if (!src_pml4[pml4i].present)
      continue;
    required_pages++; // PDPT

    PageDirPointerTableEntry *src_pdpt =
        (PageDirPointerTableEntry *)getIdentAddressOfPPN(
            src_pml4[pml4i].page_ppn);
    for (uint64 pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; ++pdpti) {
      if (!src_pdpt[pdpti].pd.present)
        continue;
      required_pages++; // PD

      PageDirEntry *src_pd =
          (PageDirEntry *)getIdentAddressOfPPN(src_pdpt[pdpti].pd.page_ppn);
      for (uint64 pdi = 0; pdi < PAGE_DIR_ENTRIES; ++pdi) {
        if (!src_pd[pdi].pt.present)
          continue;
        required_pages++; // PT

        PageTableEntry *src_pt =
            (PageTableEntry *)getIdentAddressOfPPN(src_pd[pdi].pt.page_ppn);
        for (uint64 pti = 0; pti < PAGE_TABLE_ENTRIES; ++pti) {
          if (!src_pt[pti].present)
            continue;
        }
      }
    }
  }

  for (uint64 pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; ++pml4i) {
    if (!src_pml4[pml4i].present)
      continue;

    arch_memory_lock_.release();
    uint64 new_pdpt_ppn = FrameAlloc::instance()->allocPPN();
    arch_memory_lock_.acquire();

    if (!src_pml4[pml4i].present) {
      FrameAlloc::instance()->freePPN(new_pdpt_ppn);
      continue;
    }

    PageDirPointerTableEntry *dest_pdpt =
        (PageDirPointerTableEntry *)getIdentAddressOfPPN(new_pdpt_ppn);
    memset(dest_pdpt, 0, PAGE_SIZE);

    dest_pml4[pml4i] = src_pml4[pml4i];
    dest_pml4[pml4i].page_ppn = new_pdpt_ppn;

    PageDirPointerTableEntry *src_pdpt =
        (PageDirPointerTableEntry *)getIdentAddressOfPPN(
            src_pml4[pml4i].page_ppn);
    for (uint64 pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; ++pdpti) {
      if (!src_pdpt[pdpti].pd.present)
        continue;
      assert(src_pdpt[pdpti].pd.size == 0 &&
             "ArchMemory::clonePageTables only supports mapped page tables");

      arch_memory_lock_.release();
      uint64 new_pd_ppn = FrameAlloc::instance()->allocPPN();
      arch_memory_lock_.acquire();

      if (!src_pdpt[pdpti].pd.present) {
        FrameAlloc::instance()->freePPN(new_pd_ppn);
        continue;
      }

      PageDirEntry *dest_pd = (PageDirEntry *)getIdentAddressOfPPN(new_pd_ppn);
      memset(dest_pd, 0, PAGE_SIZE);

      dest_pdpt[pdpti] = src_pdpt[pdpti];
      dest_pdpt[pdpti].pd.page_ppn = new_pd_ppn;

      PageDirEntry *src_pd =
          (PageDirEntry *)getIdentAddressOfPPN(src_pdpt[pdpti].pd.page_ppn);
      for (uint64 pdi = 0; pdi < PAGE_DIR_ENTRIES; ++pdi) {
        if (!src_pd[pdi].pt.present)
          continue;
        assert(src_pd[pdi].pt.size == 0 &&
               "ArchMemory::clonePageTables only supports 4K pages");

        arch_memory_lock_.release();
        uint64 new_pt_ppn = FrameAlloc::instance()->allocPPN();
        arch_memory_lock_.acquire();

        if (!src_pd[pdi].pt.present) {
          FrameAlloc::instance()->freePPN(new_pt_ppn);
          continue;
        }

        PageTableEntry *dest_pt =
            (PageTableEntry *)getIdentAddressOfPPN(new_pt_ppn);
        memset(dest_pt, 0, PAGE_SIZE);

        dest_pd[pdi] = src_pd[pdi];
        dest_pd[pdi].pt.page_ppn = new_pt_ppn;

        PageTableEntry *src_pt =
            (PageTableEntry *)getIdentAddressOfPPN(src_pd[pdi].pt.page_ppn);
        uint64 incref_list[PAGE_TABLE_ENTRIES];
        size_t incref_count = 0;
        for (uint64 pti = 0; pti < PAGE_TABLE_ENTRIES; ++pti) {
          PageTableEntry src_entry = src_pt[pti];
          // skip non-present entries
          if (!src_entry.present)
            continue;

          // sharing this physical page now: increase the refcount and flip
          // both PTEs to read-only COW
          bool was_shared = src_entry.shared;
          src_entry.shared = 1;
          if (!was_shared && (src_entry.writeable || src_entry.copy_on_write)) {
            src_entry.writeable = 0;     // read-only
            src_entry.copy_on_write = 1; // mark as COW
          }
          src_pt[pti] = src_entry;
          dest_pt[pti] = src_entry;
          incref_list[incref_count++] = src_entry.page_ppn;
        }

        arch_memory_lock_.release();
        for (size_t inc = 0; inc < incref_count; ++inc) {
          FrameAlloc::instance()->increfPPN(incref_list[inc]);
        }
        arch_memory_lock_.acquire();
      }
    }
  }

  // parent might still cache writable translations, so force a TLB shootdown
  // When you change a process's page tables (for example, mapping or unmapping
  // memory), the TLB may still hold stale entries. If you don't flush it, the
  // CPU could keep using old address mappings -> cause invalid memory access or
  // inconsistency.
  ArchMemory::flushTlb();

  arch_memory_lock_.release();
  return true;
}

template <typename T>
void ArchMemory::insert(pointer map_ptr, uint64 index, uint64 ppn, uint64 bzero,
                        uint64 size, uint64 user_access, uint64 writeable) {
  assert(map_ptr & ~IDENT_MAPPING_START);
  T *map = (T *)map_ptr;
  debug(A_MEMORY, "%s: page %p index %zx ppn %zx user_access %zx size %zx\n",
        __PRETTY_FUNCTION__, map, index, ppn, user_access, size);
  if (bzero) {
    memset((void *)getIdentAddressOfPPN(ppn), 0, PAGE_SIZE);
    assert(((uint64 *)map)[index] == 0);
  }
  map[index].size = size;
  map[index].writeable = writeable;
  map[index].page_ppn = ppn;
  map[index].user_access = user_access;
  map[index].present = 1;
}

bool ArchMemory::mapPage(uint64 virtual_page, uint64 physical_page,
                         uint64 user_access, uint64 writeable,
                         bool executable) {
  bool locked_before = arch_memory_lock_.heldBy() == currentThread;
  if (!locked_before)
    arch_memory_lock_.acquire();
  assert(arch_memory_lock_.heldBy() == currentThread &&
         "ArchMemory::mapPage: lock not held by current thread!");

  debug(A_MEMORY, "%zx %zx %zx %zx\n", page_map_level_4_, virtual_page,
        physical_page, user_access);
  ArchMemoryMapping first = resolveMapping(page_map_level_4_, virtual_page);
  assert((first.page_size == 0) || (first.page_size == PAGE_SIZE));

  bool need_pdpt = first.pdpt_ppn == 0;
  bool need_pd = first.pd_ppn == 0;
  bool need_pt = first.pt_ppn == 0;

  arch_memory_lock_.release();

  uint64 new_pdpt_ppn = need_pdpt ? FrameAlloc::instance()->allocPPN() : 0;
  uint64 new_pd_ppn = need_pd ? FrameAlloc::instance()->allocPPN() : 0;
  uint64 new_pt_ppn = need_pt ? FrameAlloc::instance()->allocPPN() : 0;

  arch_memory_lock_.acquire();

  ArchMemoryMapping m = resolveMapping(page_map_level_4_, virtual_page);
  assert((m.page_size == 0) || (m.page_size == PAGE_SIZE));

  uint64 unused[3] = {0, 0, 0};
  size_t unused_count = 0;

  if (m.pdpt_ppn == 0 && new_pdpt_ppn) {
    insert<PageMapLevel4Entry>((pointer)m.pml4, m.pml4i, new_pdpt_ppn, 1, 0, 1,
                               1);
    m.pdpt_ppn = new_pdpt_ppn;
  } else if (new_pdpt_ppn) {
    unused[unused_count++] = new_pdpt_ppn;
  }

  if (m.pd_ppn == 0 && new_pd_ppn) {
    insert<PageDirPointerTablePageDirEntry>(getIdentAddressOfPPN(m.pdpt_ppn),
                                            m.pdpti, new_pd_ppn, 1, 0, 1, 1);
    m.pd_ppn = new_pd_ppn;
  } else if (new_pd_ppn) {
    unused[unused_count++] = new_pd_ppn;
  }

  if (m.pt_ppn == 0 && new_pt_ppn) {
    insert<PageDirPageTableEntry>(getIdentAddressOfPPN(m.pd_ppn), m.pdi,
                                  new_pt_ppn, 1, 0, 1, 1);
    m.pt_ppn = new_pt_ppn;
  } else if (new_pt_ppn) {
    unused[unused_count++] = new_pt_ppn;
  }

  InvPageTable::instance()->lock_.acquire();
  bool mapped = false;
  if (m.page_ppn == 0 && m.pt_ppn != 0) {
    insert<PageTableEntry>(getIdentAddressOfPPN(m.pt_ppn), m.pti, physical_page,
                           0, 0, user_access, writeable);
    PageTableEntry *pt = (PageTableEntry *)getIdentAddressOfPPN(m.pt_ppn);
    pt[m.pti].execution_disabled = executable ? 0 : 1;
    mapped = true;

    // Inform the Inverted Page Table about the new mapping
    if (currentThread->type_ == Thread::USER_THREAD) {
      UserThread *user_thread = (UserThread *)currentThread;
      UserProcess *user_process = (UserProcess *)user_thread->process_;

      assert(user_process &&
             "User thread has no associated process in ArchMemory::mapPage");

      InvPageTable::instance()->insertMapping(
          physical_page, user_process, virtual_page);
    }
  }
  InvPageTable::instance()->lock_.release();

  arch_memory_lock_.release();
  for (size_t i = 0; i < unused_count; ++i) {
    if (unused[i])
      FrameAlloc::instance()->freePPN(unused[i]);
  }
  if (locked_before)
    arch_memory_lock_.acquire();
  return mapped;
}

ArchMemory::~ArchMemory() {
  if (!page_map_level_4_)
    return;
  assert(currentThread->kernel_registers_->cr3 !=
             page_map_level_4_ * PAGE_SIZE &&
         "thread deletes its own arch memory");
  arch_memory_lock_.acquire();
  PageMapLevel4Entry *pml4 =
      (PageMapLevel4Entry *)getIdentAddressOfPPN(page_map_level_4_);
  for (uint64 pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2;
       pml4i++) // free only lower half
  {
    if (pml4[pml4i].present) {
      PageDirPointerTableEntry *pdpt =
          (PageDirPointerTableEntry *)getIdentAddressOfPPN(
              pml4[pml4i].page_ppn);
      for (uint64 pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++) {
        if (pdpt[pdpti].pd.present) {
          assert(pdpt[pdpti].pd.size == 0);
          PageDirEntry *pd =
              (PageDirEntry *)getIdentAddressOfPPN(pdpt[pdpti].pd.page_ppn);
          for (uint64 pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++) {
            if (pd[pdi].pt.present) {
              assert(pd[pdi].pt.size == 0);
              PageTableEntry *pt =
                  (PageTableEntry *)getIdentAddressOfPPN(pd[pdi].pt.page_ppn);
              for (uint64 pti = 0; pti < PAGE_TABLE_ENTRIES; pti++) {
                if (pt[pti].present) {
                  uint64 data_ppn = pt[pti].page_ppn;

                  uint64 vpn =
                      calculateVPNFromPagingStructure(pml4i, pdpti, pdi, pti);
                  debug(A_MEMORY,
                        "ArchMemory::~ArchMemory: freeing data page ppn %zx "
                        "for vpn %zd\n",
                        data_ppn, vpn);

                  // Remove from IPT
                  InvPageTable::instance()->lock_.acquire();
                  InvPageTable::instance()
                      ->findAndRemoveMappingFromIPTEntry(&pt[pti], this, vpn);
                  InvPageTable::instance()->lock_.release();

                  pt[pti].present = 0;
                  arch_memory_lock_.release();
                  FrameAlloc::instance()->freePPN(data_ppn);
                  arch_memory_lock_.acquire();

                  pml4 = (PageMapLevel4Entry *)getIdentAddressOfPPN(
                      page_map_level_4_);
                  pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(
                      pml4[pml4i].page_ppn);
                  pd = (PageDirEntry *)getIdentAddressOfPPN(
                      pdpt[pdpti].pd.page_ppn);
                  pt = (PageTableEntry *)getIdentAddressOfPPN(
                      pd[pdi].pt.page_ppn);
                }
                // if the entry is swapped out, we should also free its swap
                // slot
                else if (pt[pti].swapped_out) {
                  size_t vpn =
                      calculateVPNFromPagingStructure(pml4i, pdpti, pdi, pti);
                  InvPageTable::instance()->lock_.acquire();
                  InvPageTable::instance()
                      ->findAndRemoveMappingFromIPTEntry(&pt[pti], this, vpn);
                  InvPageTable::instance()->lock_.release();
                }
              }
              uint64 pt_ppn = pd[pdi].pt.page_ppn;
              pd[pdi].pt.present = 0;
              arch_memory_lock_.release();
              FrameAlloc::instance()->freePPN(pt_ppn);
              arch_memory_lock_.acquire();
              pml4 =
                  (PageMapLevel4Entry *)getIdentAddressOfPPN(page_map_level_4_);
              pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(
                  pml4[pml4i].page_ppn);
              pd =
                  (PageDirEntry *)getIdentAddressOfPPN(pdpt[pdpti].pd.page_ppn);
            }
          }
          uint64 pd_ppn = pdpt[pdpti].pd.page_ppn;
          pdpt[pdpti].pd.present = 0;
          arch_memory_lock_.release();
          FrameAlloc::instance()->freePPN(pd_ppn);
          arch_memory_lock_.acquire();
          pml4 = (PageMapLevel4Entry *)getIdentAddressOfPPN(page_map_level_4_);
          pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(
              pml4[pml4i].page_ppn);
        }
      }
      uint64 pdpt_ppn = pml4[pml4i].page_ppn;
      pml4[pml4i].present = 0;
      arch_memory_lock_.release();
      FrameAlloc::instance()->freePPN(pdpt_ppn);
      arch_memory_lock_.acquire();
      pml4 = (PageMapLevel4Entry *)getIdentAddressOfPPN(page_map_level_4_);
    }
  }
  arch_memory_lock_.release();
  FrameAlloc::instance()->freePPN(page_map_level_4_);
}

pointer ArchMemory::checkAddressValid(uint64 vaddress_to_check) {
  if (vaddress_to_check >= USER_BREAK && vaddress_to_check < KERNEL_START) {
    debug(A_MEMORY, "checkAddressValid %zx non-canonical -> false\n",
          vaddress_to_check);
    return 0;
  }

  ArchMemoryMapping m =
      resolveMapping(page_map_level_4_, vaddress_to_check / PAGE_SIZE);
  if (m.page != 0) {
    debug(A_MEMORY, "checkAddressValid %zx and %zx -> true\n",
          page_map_level_4_, vaddress_to_check);
    return m.page | (vaddress_to_check % m.page_size);
  } else {
    debug(A_MEMORY, "checkAddressValid %zx and %zx -> false\n",
          page_map_level_4_, vaddress_to_check);
    return 0;
  }
}

const ArchMemoryMapping ArchMemory::resolveMapping(uint64 vpage) {
  assert(arch_memory_lock_.heldBy() == currentThread &&
         "ArchMemory::unmapPage: lock not held by current thread!");

  return resolveMapping(page_map_level_4_, vpage);
}

const ArchMemoryMapping ArchMemory::resolveMapping(uint64 pml4, uint64 vpage) {
  assert(
      (vpage * PAGE_SIZE < USER_BREAK || vpage * PAGE_SIZE >= KERNEL_START) &&
      "This is not a valid vpn! Did you pass an address to resolveMapping?");
  ArchMemoryMapping m;

  m.pti = vpage;
  m.pdi = m.pti / PAGE_TABLE_ENTRIES;
  m.pdpti = m.pdi / PAGE_DIR_ENTRIES;
  m.pml4i = m.pdpti / PAGE_DIR_POINTER_TABLE_ENTRIES;

  m.pti %= PAGE_TABLE_ENTRIES;
  m.pdi %= PAGE_DIR_ENTRIES;
  m.pdpti %= PAGE_DIR_POINTER_TABLE_ENTRIES;
  m.pml4i %= PAGE_MAP_LEVEL_4_ENTRIES;

  assert(pml4 < FrameAlloc::instance()->getTotalNumPages());
  m.pml4 = (PageMapLevel4Entry *)getIdentAddressOfPPN(pml4);
  m.pdpt = 0;
  m.pd = 0;
  m.pt = 0;
  m.page = 0;
  m.pml4_ppn = pml4;
  m.pdpt_ppn = 0;
  m.pd_ppn = 0;
  m.pt_ppn = 0;
  m.page_ppn = 0;
  m.page_size = 0;
  if (m.pml4[m.pml4i].present) {
    m.pdpt_ppn = m.pml4[m.pml4i].page_ppn;
    m.pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(
        m.pml4[m.pml4i].page_ppn);
    if (m.pdpt[m.pdpti].pd.present && !m.pdpt[m.pdpti].pd.size) // 1gb page ?
    {
      m.pd_ppn = m.pdpt[m.pdpti].pd.page_ppn;
      if (m.pd_ppn > FrameAlloc::instance()->getTotalNumPages()) {
        debug(A_MEMORY, "%zx\n", m.pd_ppn);
      }
      assert(m.pd_ppn < FrameAlloc::instance()->getTotalNumPages());
      m.pd = (PageDirEntry *)getIdentAddressOfPPN(m.pdpt[m.pdpti].pd.page_ppn);
      if (m.pd[m.pdi].pt.present && !m.pd[m.pdi].pt.size) // 2mb page ?
      {
        m.pt_ppn = m.pd[m.pdi].pt.page_ppn;
        assert(m.pt_ppn < FrameAlloc::instance()->getTotalNumPages());
        m.pt = (PageTableEntry *)getIdentAddressOfPPN(m.pd[m.pdi].pt.page_ppn);
        if (m.pt[m.pti].present) {
          m.page = getIdentAddressOfPPN(m.pt[m.pti].page_ppn);
          m.page_ppn = m.pt[m.pti].page_ppn;
          assert(m.page_ppn < FrameAlloc::instance()->getTotalNumPages());
          m.page_size = PAGE_SIZE;
        }
      } else if (m.pd[m.pdi].page.present) {
        m.page_size = PAGE_SIZE * PAGE_TABLE_ENTRIES;
        m.page_ppn = m.pd[m.pdi].page.page_ppn;
        m.page = getIdentAddressOfPPN(m.pd[m.pdi].page.page_ppn);
      }
    } else if (m.pdpt[m.pdpti].page.present) {
      m.page_size = PAGE_SIZE * PAGE_TABLE_ENTRIES * PAGE_DIR_ENTRIES;
      m.page_ppn = m.pdpt[m.pdpti].page.page_ppn;
      assert(m.page_ppn < FrameAlloc::instance()->getTotalNumPages());
      m.page = getIdentAddressOfPPN(m.pdpt[m.pdpti].page.page_ppn);
    }
  }
  return m;
}

uint64 ArchMemory::get_PPN_Of_VPN_In_KernelMapping(size_t virtual_page,
                                                   size_t *physical_page,
                                                   size_t *physical_pte_page) {
  ArchMemoryMapping m = resolveMapping(
      ((uint64)VIRTUAL_TO_PHYSICAL_BOOT(kernel_page_map_level_4) / PAGE_SIZE),
      virtual_page);
  if (physical_page)
    *physical_page = m.page_ppn;
  if (physical_pte_page)
    *physical_pte_page = m.pt_ppn;
  return m.page_size;
}

void ArchMemory::mapKernelPage(size_t virtual_page, size_t physical_page) {
  ArchMemoryMapping mapping = resolveMapping(
      ((uint64)VIRTUAL_TO_PHYSICAL_BOOT(kernel_page_map_level_4) / PAGE_SIZE),
      virtual_page);
  PageMapLevel4Entry *pml4 = kernel_page_map_level_4;
  assert(pml4[mapping.pml4i].present);
  PageDirPointerTableEntry *pdpt =
      (PageDirPointerTableEntry *)getIdentAddressOfPPN(
          pml4[mapping.pml4i].page_ppn);
  assert(pdpt[mapping.pdpti].pd.present);
  PageDirEntry *pd =
      (PageDirEntry *)getIdentAddressOfPPN(pdpt[mapping.pdpti].pd.page_ppn);
  assert(pd[mapping.pdi].pt.present);
  PageTableEntry *pt =
      (PageTableEntry *)getIdentAddressOfPPN(pd[mapping.pdi].pt.page_ppn);
  assert(!pt[mapping.pti].present);
  pt[mapping.pti].writeable = 1;
  pt[mapping.pti].page_ppn = physical_page;
  pt[mapping.pti].present = 1;
  asm volatile("movq %%cr3, %%rax; movq %%rax, %%cr3;" ::: "%rax");
}

void ArchMemory::unmapKernelPage(size_t virtual_page) {
  ArchMemoryMapping mapping = resolveMapping(
      ((uint64)VIRTUAL_TO_PHYSICAL_BOOT(kernel_page_map_level_4) / PAGE_SIZE),
      virtual_page);
  PageMapLevel4Entry *pml4 = kernel_page_map_level_4;
  assert(pml4[mapping.pml4i].present);
  PageDirPointerTableEntry *pdpt =
      (PageDirPointerTableEntry *)getIdentAddressOfPPN(
          pml4[mapping.pml4i].page_ppn);
  assert(pdpt[mapping.pdpti].pd.present);
  PageDirEntry *pd =
      (PageDirEntry *)getIdentAddressOfPPN(pdpt[mapping.pdpti].pd.page_ppn);
  assert(pd[mapping.pdi].pt.present);
  PageTableEntry *pt =
      (PageTableEntry *)getIdentAddressOfPPN(pd[mapping.pdi].pt.page_ppn);
  assert(pt[mapping.pti].present);
  pt[mapping.pti].present = 0;
  pt[mapping.pti].writeable = 0;
  FrameAlloc::instance()->freePPN(pt[mapping.pti].page_ppn);
  asm volatile("movq %%cr3, %%rax; movq %%rax, %%cr3;" ::: "%rax");
}

PageMapLevel4Entry *ArchMemory::getRootOfKernelPagingStructure() {
  return kernel_page_map_level_4;
}

void ArchMemory::flushTlb() {
  asm volatile("movq %%cr3, %%rax; movq %%rax, %%cr3;" ::: "%rax", "memory");
}

bool ArchMemory::isAccessed(uint64 virtual_page) {
  bool locked_before = arch_memory_lock_.isHeldBy(currentThread);
  if (!locked_before && !arch_memory_lock_.acquireNonBlocking())
    return false;

  ArchMemoryMapping mapping = resolveMapping(virtual_page);
  bool accessed = false;
  if (mapping.pt && mapping.pt[mapping.pti].present) {
    accessed = mapping.pt[mapping.pti].accessed;
  }

  if (!locked_before)
    arch_memory_lock_.release();
  return accessed;
}

void ArchMemory::clearAccessed(uint64 virtual_page) {
  bool locked_before = arch_memory_lock_.isHeldBy(currentThread);
  if (!locked_before && !arch_memory_lock_.acquireNonBlocking())
    return;

  ArchMemoryMapping mapping = resolveMapping(virtual_page);
  if (mapping.pt && mapping.pt[mapping.pti].present) {
    mapping.pt[mapping.pti].accessed = 0;
  }

  if (!locked_before)
    arch_memory_lock_.release();
}

uint64 ArchMemory::calculateVPNFromPagingStructure(uint64 pml4i, uint64 pdpti,
                                                   uint64 pdi, uint64 pti) {
  return (pml4i * PAGE_DIR_POINTER_TABLE_ENTRIES * PAGE_DIR_ENTRIES *
          PAGE_TABLE_ENTRIES) +
         (pdpti * PAGE_DIR_ENTRIES * PAGE_TABLE_ENTRIES) +
         (pdi * PAGE_TABLE_ENTRIES) + pti;
}
