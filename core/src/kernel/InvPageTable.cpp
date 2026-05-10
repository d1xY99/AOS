#include "InvPageTable.h"
#include "FrameAlloc.h"
#include "SwapEngine.h"
#include "Thread.h"
#include "debug.h"
#include "paging-definitions.h"

#define PRINT_IPT 0

InvPageTable *InvPageTable::instance_ = nullptr;

InvPageTable *InvPageTable::instance() {
  if (unlikely(!instance_))
    instance_ = new InvPageTable();
  return instance_;
}

InvPageTable::InvPageTable()
    : lock_("InvPageTable::lock_") {
  debug(IPT_MANAGER, "Creating Inverted Page Table Manager...\n");

  total_pages_ = FrameAlloc::instance()->getTotalNumPages();
  ipt_entries_ = new InvPageTableEntry *[total_pages_];

  for (size_t i = 0; i < total_pages_; ++i) {
    ipt_entries_[i] = new InvPageTableEntry();
  }

  debug(IPT_MANAGER, "Inverted Page Table Manager created with %u entries\n",
        total_pages_);
}

namespace {
// If only one mapping remains, drop COW/read-only so the last process can
// write.
void restoreWriteableForLastMapping(InvPageTableEntry *entry) {
  if (!entry || entry->refcount != 1 || entry->mappings_.size() != 1)
    return;

  auto it = entry->mappings_.begin();
  UserProcess *process = it->first;
  if (!process || !process->loader_ || it->second.size() != 1)
    return;

  size_t vpn = it->second.front();
  ArchMemory &arch_memory = process->loader_->arch_memory_;
  ArchMemoryMapping m =
      ArchMemory::resolveMapping(arch_memory.page_map_level_4_, vpn);
  if (!m.pt)
    return;

  PageTableEntry &pte = m.pt[m.pti];
  if (!pte.present && !pte.swapped_out)
    return;

  pte.writeable = 1;
  pte.copy_on_write = 0;
  pte.shared = 0;
  ArchMemory::flushTlb();
}
} // namespace

void InvPageTable::printIptEntry(InvPageTableEntry *entry,
                                             size_t entry_index, bool swapped) {

  if (!PRINT_IPT)
    return;

  assert(lock_.heldBy() == currentThread &&
         "InvPageTable::printIptEntry: Lock not held by current "
         "thread");

  debug(IPT_MANAGER, "+-------------------------------+\n");
  if (swapped) {
    debug(IPT_MANAGER, "| IPT Entry for SWAP SPACE %-10zu (SWAPPED) |\n",
          entry_index);
  } else {
    debug(IPT_MANAGER, "| IPT Entry for PPN %-10zu (IN RAM)  |\n", entry_index);
  }
  debug(IPT_MANAGER, "| size of mappings: %-10zu  |\n",
        entry->mappings_.size());
  debug(IPT_MANAGER, "| refcount: %-18zu |\n", entry->refcount);
  debug(IPT_MANAGER, "+-------------------------------+\n");

  for (const auto &mapping : entry->mappings_) {
    UserProcess *process = mapping.first;
    const aostl::list<size_t> &vpn_list = mapping.second;

    debug(IPT_MANAGER, "|-- Process PID %zu\n", process->getPID());

    for (size_t vpn : vpn_list) {
      debug(IPT_MANAGER, "|   |-- VPN %zu\n", vpn);
    }

    debug(IPT_MANAGER, "|\n");
  }

  debug(IPT_MANAGER, "+-------------------------------+\n");
}

void InvPageTable::insertMapping(size_t ppn, UserProcess *process,
                                             size_t vpn) {
  assert(lock_.heldBy() == currentThread &&
         "InvPageTable::insertMapping: Lock not held by current "
         "thread");

  debug(IPT_MANAGER, "Inserting mapping: PPN %zu -> Process PID %zu, VPN %zu\n",
        ppn, process->getPID(), vpn);

  if (ppn >= total_pages_) {
    debug(IPT_MANAGER,
          "InvPageTable::insertMapping: Invalid PPN %zu\n", ppn);
    assert(false &&
           "InvPageTable::insertMapping: PPN out of range");
  }

  InvPageTableEntry *entry = ipt_entries_[ppn];

  // check if the entry exists or is set to nullptr
  if (entry == nullptr) {
    debug(IPT_MANAGER,
          "InvPageTable::insertMapping: IPT entry for PPN %zu is "
          "null\n",
          ppn);
    debug(IPT_MANAGER, "InvPageTable::insertMapping: creatting new "
                       "IPT entry for\n");
    entry = new InvPageTableEntry();
    ipt_entries_[ppn] = entry;
  }

  entry->insertMappingToEntry(process, vpn);

  entry->referenced_ = true;

  debug(IPT_MANAGER, "Inserted mapping: PPN %zu -> Process PID %zu, VPN %zu\n",
        ppn, process->getPID(), vpn);
  printIptEntry(entry, ppn, entry->swapped);
}

void InvPageTableEntry::insertMappingToEntry(UserProcess *p, size_t vpn) {
  assert(InvPageTable::instance()->lock_.heldBy() ==
             currentThread &&
         "InvPageTableEntry::insertMappingFromEntry: Lock not held by "
         "current thread");
  if (mappings_.empty()) {
    age_counter_ = 0;
  }
  debug(IPT_MANAGER,
        "InvPageTableEntry::insertMappingToEntry: Inserting mapping for "
        "Process PID %zu, VPN %zu\n",
        p->getPID(), vpn);
  mappings_[p].push_back(vpn);
  ++refcount;
}

void InvPageTableEntry::removeMappingFromEntry(UserProcess *p,
                                                    size_t vpn) {

  assert(InvPageTable::instance()->lock_.heldBy() ==
             currentThread &&
         "InvPageTableEntry::removeMappingFromEntry: Lock not held by "
         "current thread");
  assert(isFree() == false &&
         "InvPageTableEntry::removeMappingFromEntry: Trying to remove "
         "mapping from a free IPT entry");

  auto it = mappings_.find(p);
  if (it != mappings_.end()) {
    aostl::list<size_t> &vpn_list = it->second;
    debug(
        IPT_MANAGER,
        "InvPageTableEntry::removeMappingFromEntry: Removing mapping for "
        "Process PID %zu, VPN %zu\n",
        p->getPID(), vpn);
    bool removed = false;
    for (auto it_list = vpn_list.begin(); it_list != vpn_list.end();) {
      if (*it_list == vpn) {
        it_list = vpn_list.erase(it_list);
        removed = true;
        break;
      }
      ++it_list;
    }
    assert(removed &&
           "InvPageTableEntry::removeMappingFromEntry: VPN not found");
    assert(
        refcount > 0 &&
        "InvPageTableEntry::removeMappingFromEntry: Refcount underflow");
    --refcount;
    if (vpn_list.empty()) {
      mappings_.erase(it);
    }
    if (mappings_.empty()) {
      age_counter_ = 0;
    }
    return;
  }

  assert(false && "Mapping to remove not found in IPT entry");
}

InvPageTableEntry *InvPageTable::getIptEntry(size_t ppn) {
  assert(lock_.heldBy() == currentThread &&
         "InvPageTable::getIptEntry: Lock not held by current "
         "thread");

  // ppns lower than total_pages_ / 2 are reserved for kernel use
  if (ppn < (total_pages_ / 2) || ppn >= total_pages_) {
    debug(IPT_MANAGER,
          "InvPageTable::getIptEntry: Invalid PPN %zu\n", ppn);
    assert(false && "InvPageTable::getIptEntry: PPN out of range");
  }

  return ipt_entries_[ppn];
}

void InvPageTable::setEntryToNull(size_t ppn) {
  assert(lock_.heldBy() == currentThread &&
         "InvPageTable::setEntryToFree: Lock not held by current "
         "thread");

  debug(IPT_MANAGER,
        "InvPageTable::setEntryToFree: Setting IPT entry for PPN "
        "%zu to free\n",
        ppn);

  if (ppn >= total_pages_) {
    debug(IPT_MANAGER,
          "InvPageTable::setEntryToFree: Invalid PPN %zu\n", ppn);
    assert(false &&
           "InvPageTable::setEntryToFree: PPN out of range");
  }

  ipt_entries_[ppn] = nullptr;
}

void InvPageTable::setIPTEntry(size_t ppn,
                                           InvPageTableEntry *entry) {
  assert(lock_.heldBy() == currentThread &&
         "InvPageTable::setIPTEntry: Lock not held by current "
         "thread");

  assert(ipt_entries_[ppn] == nullptr &&
         "InvPageTable::setIPTEntry: IPT entry is not null");
  assert(entry != nullptr &&
         "InvPageTable::setIPTEntry: Entry to set is null");

  debug(
      IPT_MANAGER,
      "InvPageTable::setIPTEntry: Setting IPT entry for PPN %zu\n",
      ppn);

  if (ppn >= total_pages_) {
    debug(IPT_MANAGER,
          "InvPageTable::setIPTEntry: Invalid PPN %zu\n", ppn);
    assert(false && "InvPageTable::setIPTEntry: PPN out of range");
  }

  ipt_entries_[ppn] = entry;
}

// IPT Entry could be in RAM or swapped out to disk
InvPageTableEntry *
InvPageTable::findIPTEntry(PageTableEntry *pte) {
  assert(lock_.heldBy() == currentThread &&
         "InvPageTable::findIPTEntry: Lock not held by current "
         "thread");

  debug(IPT_MANAGER,
        "InvPageTable::findIPTEntry: present=%d, swapped_out=%d\n",
        pte->present, pte->swapped_out);

  InvPageTableEntry *ipt_entry = nullptr;

  // find the entry in RAM
  if (pte->present) {
    ipt_entry = getIptEntry(pte->page_ppn);
  }
  // if not present, find it in swapped out entries
  else if (pte->swapped_out) {
    ipt_entry = SwapEngine::instance()->getSwappedOutIptEntry(pte->page_ppn);
  }

  if (ipt_entry != nullptr) {
    printIptEntry(ipt_entry, pte->page_ppn, pte->swapped_out);
    return ipt_entry;
  }

  assert(false &&
         "InvPageTable::findIPTEntry: IPT entry not found");
}

void InvPageTable::findAndRemoveMappingFromIPTEntry(
    PageTableEntry *pte, ArchMemory *arch_memory, size_t vpn) {
  assert(lock_.heldBy() == currentThread &&
         "InvPageTable::findAndRemoveMappingFromIPTEntry: Lock not "
         "held by current thread");

  InvPageTableEntry *ipt_entry = findIPTEntry(pte);

  for (auto &mapping : ipt_entry->mappings_) {
    UserProcess *process = mapping.first;
    if (&process->loader_->arch_memory_ == arch_memory) {
      ipt_entry->removeMappingFromEntry(process, vpn);
      debug(IPT_MANAGER,
            "InvPageTable::findAndRemoveMappingFromIPTEntry: "
            "Removed mapping for Process PID %zu, VPN %zu\n",
            process->getPID(), vpn);
      printIptEntry(ipt_entry, pte->page_ppn, pte->swapped_out);
      restoreWriteableForLastMapping(ipt_entry);
      return;
    }
  }
}

void InvPageTable::deleteIPTEntry(size_t ppn) {
  assert(lock_.heldBy() == currentThread &&
         "InvPageTable::deleteIPTEntry: Lock not held by current "
         "thread");

  debug(IPT_MANAGER,
        "InvPageTable::deleteIPTEntry: Deleting IPT entry for PPN "
        "%zu\n",
        ppn);

  if (ppn >= total_pages_) {
    debug(IPT_MANAGER,
          "InvPageTable::deleteIPTEntry: Invalid PPN %zu\n", ppn);
    assert(false &&
           "InvPageTable::deleteIPTEntry: PPN out of range");
  }

  InvPageTableEntry *entry = ipt_entries_[ppn];
  delete entry;
  ipt_entries_[ppn] = nullptr;
}
