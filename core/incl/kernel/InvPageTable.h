#pragma once

#include "ArchMemory.h"
#include "Mutex.h"
#include "UserProcess.h"
#include "debug.h"
#include "paging-definitions.h"
#include "types.h"
#include "ulist.h"
#include "umap.h"

struct InvPageTableEntry {
  aostl::map<UserProcess *, aostl::list<size_t>> mappings_;
  size_t refcount = 0;

  bool isFree() const { return mappings_.empty(); }

  bool swapped = false;

  void insertMappingToEntry(UserProcess *p, size_t vpn);
  void removeMappingFromEntry(UserProcess *p, size_t vpn);

  // PRA Related data

  // AGING
  uint32 age_counter_ = 0;

  // CLOCK
  bool referenced_ = false;
};

/**
 * The Inverted Page Table (IPT)
 *
 * This class manages the inverted page table, which keeps track of the mapping
 * between physical pages and virtual pages across all processes.
 */
class InvPageTable {
public:
  static InvPageTable *instance();

  Mutex lock_;
  InvPageTableEntry **ipt_entries_;

  void insertMapping(size_t ppn, UserProcess *process, size_t vpn);

  // IPT entry could on RAM or swapped out to disk
  InvPageTableEntry *findIPTEntry(PageTableEntry *pte);

  void findAndRemoveMappingFromIPTEntry(PageTableEntry *pte,
                                        ArchMemory *arch_memory, size_t vpn);

  void printIptEntry(InvPageTableEntry *entry, size_t entry_index,
                     bool swapped);

  InvPageTableEntry *getIptEntry(size_t ppn);

  void setIPTEntry(size_t ppn, InvPageTableEntry *entry);

  // Null means the entry is free and got swapped out before
  void setEntryToNull(size_t ppn);

  void deleteIPTEntry(size_t ppn);

private:
  InvPageTable();
  static InvPageTable *instance_;
  uint32 total_pages_;

  friend class SwapEngine;
};
