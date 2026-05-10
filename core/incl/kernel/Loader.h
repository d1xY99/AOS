#pragma once

#include "ArchMemory.h"
#include "ElfFormat.h"
#include "Mutex.h"
#include "types.h"
#include <ulist.h>
#include <uvector.h>

class KernelDebugInfo;

struct TlsInfo {
  void *ident_address;
  Elf::Phdr phdr;
  bool found;
};

class Loader {
public:
  Loader(ssize_t fd);
  Loader(const Loader &other, ssize_t fd_override = -1);
  ~Loader();

  Loader &operator=(const Loader &) = delete;

  /**
   * Loads the ehdr and phdrs from the executable and (optionally) loads debug
   * info.
   * @return true if this was successful, false otherwise
   */
  bool loadExecutableAndInitProcess();

  /**
   * Loads one binary page by its virtual address:
   * Gets a free physical page, copies the contents of the binary to it, and
   * then maps it.
   * @param virtual_address virtual address where to find the page to load
   */
  void loadPage(pointer virtual_address);

  KernelDebugInfo const *getDebugInfos() const;

  void *getEntryFunction() const;
  bool isValid() const { return clone_ok_; }

  ArchMemory arch_memory_;

  TlsInfo tls_info_;

private:
  /**
   *reads ELF-headers from the executable
   * @return true if this was successful, false otherwise
   */
  bool readHeaders();

  /**
   * clean up and sort the elf headers for faster access.
   * @return true in case the headers could be prepared
   */
  bool prepareHeaders();

  bool loadDebugInfoIfAvailable();

  bool readFromBinary(char *buffer, l_off_t position, size_t length);

  size_t fd_;
  Elf::Ehdr *hdr_;
  aostl::list<Elf::Phdr> phdrs_;

  Mutex program_binary_lock_;

  KernelDebugInfo *userspace_debug_info_;
  bool clone_ok_;
};
