#include "Loader.h"
#include "ArchMemory.h"
#include "ArchThreads.h"
#include "File.h"
#include "FileDescriptor.h"
#include "FrameAlloc.h"
#include "AOSDebugInfo.h"
#include "Scheduler.h"
#include "KernelDebugInfo.h"
#include "Syscall.h"
#include "FsOps.h"
#include "debug.h"
#include "kstring.h"
#include "paging-definitions.h"
#include <assert.h>

Loader::Loader(ssize_t fd)
    : fd_(fd), hdr_(0), phdrs_(),
      program_binary_lock_("Loader::program_binary_lock_"),
      userspace_debug_info_(0), clone_ok_(true) {
  tls_info_.found = false;
}

Loader::Loader(const Loader &other, ssize_t fd_override)
    : arch_memory_(other.arch_memory_),
      fd_(fd_override >= 0 ? fd_override : other.fd_), hdr_(0),
      phdrs_(other.phdrs_),
      program_binary_lock_("Loader::program_binary_lock_"),
      userspace_debug_info_(0), clone_ok_(true) {
  if (other.hdr_)
    hdr_ = new Elf::Ehdr(*other.hdr_);

  tls_info_ = other.tls_info_;

  clone_ok_ = arch_memory_.cloneSuccessful();
  if (!clone_ok_ && hdr_) {
    delete hdr_;
    hdr_ = nullptr;
  }
}

Loader::~Loader() {
  delete userspace_debug_info_;
  delete hdr_;
  userspace_debug_info_ = nullptr;
  hdr_ = nullptr;
}

void Loader::loadPage(pointer virtual_address) {
  debug(LOADER, "Loader:loadPage: Request to load the page for address %p.\n",
        (void *)virtual_address);
  const pointer virt_page_start_addr = virtual_address & ~(PAGE_SIZE - 1);
  const pointer virt_page_end_addr = virt_page_start_addr + PAGE_SIZE;
  bool found_page_content = false;
  bool page_writeable = false;
  // get a new page for the mapping
  size_t ppn = FrameAlloc::instance()->allocPPN();

  program_binary_lock_.acquire();

  // Iterate through all sections and load the ones intersecting into the page.
  for (aostl::list<Elf::Phdr>::iterator it = phdrs_.begin(); it != phdrs_.end();
       it++) {
    if ((*it).p_vaddr >= virt_page_end_addr)
      continue;

    bool intersects_file =
        (*it).p_vaddr + (*it).p_filesz > virt_page_start_addr;
    bool intersects_mem = (*it).p_vaddr + (*it).p_memsz > virt_page_start_addr;
    if (!intersects_file && !intersects_mem)
      continue;

    if ((*it).p_flags & Elf::PF_W)
      page_writeable = true;

    if (intersects_file) {
      const pointer virt_start_addr =
          aostl::max(virt_page_start_addr, (*it).p_vaddr);
      const size_t virt_offs_on_page = virt_start_addr - virt_page_start_addr;
      const l_off_t bin_start_addr =
          (*it).p_offset + (virt_start_addr - (*it).p_vaddr);
      const size_t bytes_to_load =
          aostl::min(virt_page_end_addr, (*it).p_vaddr + (*it).p_filesz) -
          virt_start_addr;
      if (readFromBinary((char *)ArchMemory::getIdentAddressOfPPN(ppn) +
                             virt_offs_on_page,
                         bin_start_addr, bytes_to_load)) {
        program_binary_lock_.release();
        FrameAlloc::instance()->freePPN(ppn);
        debug(LOADER, "ERROR! Some parts of the content could not be loaded "
                      "from the binary.\n");
        Syscall::exit(999);
      }
      found_page_content = true;
    } else if (intersects_mem) {
      found_page_content = true;
    }
  }
  program_binary_lock_.release();

  if (!found_page_content) {
    FrameAlloc::instance()->freePPN(ppn);
    debug(LOADER,
          "Loader::loadPage: ERROR! No section refers to the given address.\n");
    Syscall::exit(666);
  }

  size_t vpn = virt_page_start_addr / PAGE_SIZE;
  bool page_mapped =
      arch_memory_.mapPage(vpn, ppn, true, page_writeable ? 1 : 0);

  if (!page_mapped) {
    debug(LOADER,
          "Loader::loadPage: The page has been mapped by someone else.\n");
    FrameAlloc::instance()->freePPN(ppn);
  }

  debug(LOADER,
        "Loader::loadPage: Load request for address %p has been successfully "
        "finished.\n",
        (void *)virtual_address);
}

bool Loader::readFromBinary(char *buffer, l_off_t position, size_t length) {
  assert(program_binary_lock_.isHeldBy(currentThread));
  FsOps::lseek(fd_, position, SEEK_SET);
  return FsOps::read(fd_, buffer, length) - (ssize_t)length;
}

bool Loader::readHeaders() {
  ScopeLock lock(program_binary_lock_);
  hdr_ = new Elf::Ehdr;

  if (readFromBinary((char *)hdr_, 0, sizeof(Elf::Ehdr))) {
    debug(LOADER,
          "Loader::readHeaders: ERROR! The headers could not be loaded.\n");
    return false;
  }

  // checking elf-magic-numbers, format (32/64bit) and a few more things
  if (!Elf::headerCorrect(hdr_)) {
    debug(LOADER, "Loader::readHeaders: ERROR! The headers are invalid.\n");
    return false;
  }

  if (sizeof(Elf::Phdr) != hdr_->e_phentsize) {
    debug(LOADER, "Expected program header size does not match advertised "
                  "program header size\n");
    return false;
  }
  phdrs_.resize(hdr_->e_phnum);
  if (readFromBinary(reinterpret_cast<char *>(&phdrs_[0]), hdr_->e_phoff,
                     hdr_->e_phnum * sizeof(Elf::Phdr))) {
    return false;
  }
  if (!prepareHeaders()) {
    debug(LOADER, "Loader::readHeaders: ERROR! There are no valid sections in "
                  "the binary.\n");
    return false;
  }
  return true;
}

void *Loader::getEntryFunction() const { return (void *)hdr_->e_entry; }

bool Loader::loadExecutableAndInitProcess() {
  debug(LOADER,
        "Loader::loadExecutableAndInitProcess: going to load an executable\n");
  clone_ok_ = false;

  if (!readHeaders())
    return false;

  debug(LOADER, "loadExecutableAndInitProcess: Entry: %zx, num Sections %zx\n",
        hdr_->e_entry, (size_t)hdr_->e_phnum);
  if (LOADER & OUTPUT_ADVANCED)
    Elf::printElfHeader(*hdr_);

  if (USERTRACE & OUTPUT_ENABLED)
    loadDebugInfoIfAvailable();

  clone_ok_ = true;
  return true;
}

bool Loader::loadDebugInfoIfAvailable() {
  assert(!userspace_debug_info_ && "You may not load User Debug Info twice!");

  debug(USERTRACE, "loadDebugInfoIfAvailable start\n");
  if (sizeof(Elf::Shdr) != hdr_->e_shentsize) {
    debug(USERTRACE, "Expected section header size does not match advertised "
                     "section header size\n");
    return false;
  }

  ScopeLock lock(program_binary_lock_);

  aostl::vector<Elf::Shdr> section_headers;
  section_headers.resize(hdr_->e_shnum);
  if (readFromBinary(reinterpret_cast<char *>(&section_headers[0]),
                     hdr_->e_shoff, hdr_->e_shnum * sizeof(Elf::Shdr))) {
    debug(USERTRACE, "Failed to load section headers!\n");
    return false;
  }

  // now that we have loaded the section headers, we want to find and load the
  // section that contains the section names in the simple case this section
  // name section is only 0xFF00 bytes long, in that case loading is simple. we
  // only support this case for now

  size_t section_name_section = hdr_->e_shstrndx;
  size_t section_name_size = section_headers[section_name_section].sh_size;
  aostl::vector<char> section_names(section_name_size);

  if (readFromBinary(&section_names[0],
                     section_headers[section_name_section].sh_offset,
                     section_name_size)) {
    debug(USERTRACE, "Failed to load section name section\n");
    return false;
  }

  // now that we have names we read through all the sections
  // and load the two we're interested in

  char *aos_data = 0;
  size_t aos_data_size = 0;

  for (Elf::Shdr const &section : section_headers) {
    if (section.sh_name) {
      if (!strcmp(&section_names[section.sh_name], ".aosdbg")) {
        debug(USERTRACE, "Found AOSDbg Infos\n");
        size_t size = section.sh_size;
        if (size) {
          aos_data = new char[size];
          aos_data_size = size;
          if (readFromBinary(aos_data, section.sh_offset, size)) {
            debug(USERTRACE, "Could not read aosdbg section!\n");
            delete[] aos_data;
            aos_data = 0;
          }
        } else {
          debug(USERTRACE, "AOSDbg Infos are empty\n");
          return false;
        }
      }
    }
  }

  if (!aos_data) {
    debug(USERTRACE, "Failed to load necessary debug data!\n");
    return false;
  }

  userspace_debug_info_ =
      new AOSDebugInfo(aos_data, aos_data + aos_data_size);
  return true;
}

KernelDebugInfo const *Loader::getDebugInfos() const {
  return userspace_debug_info_;
}

bool Loader::prepareHeaders() {

  debug(LOADER, "Loader::prepareHeaders: Preparing %zu program headers.\n",
        phdrs_.size());

  // START TLS SEGMENT HANDLING
  // check if tls segment exists
  bool has_tls_segment_ = false;
  for (aostl::list<Elf::Phdr>::iterator it = phdrs_.begin(); it != phdrs_.end();
       it++) {
    if ((*it).p_type == Elf::PT_TLS) {
      has_tls_segment_ = true;
      break;
    }
  }

  debug(LOADER, "Loader::prepareHeaders: Found TLS segment -> %s\n",
        has_tls_segment_ ? "yes" : "no");

  // debug the tls segment
  if (has_tls_segment_) {
    for (aostl::list<Elf::Phdr>::iterator it = phdrs_.begin();
         it != phdrs_.end(); it++) {
      if ((*it).p_type == Elf::PT_TLS) {
        debug(LOADER,
              "Loader::prepareHeaders: TLS Segment: vaddr: %zx, memsz: %zx, "
              "filesz: %zx, align: %zx\n",
              (*it).p_vaddr, (*it).p_memsz, (*it).p_filesz, (*it).p_align);

        // map a page for the tls segment
        size_t tls_pages = ((*it).p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;

        if (tls_pages > 1) {
          assert(false &&
                 "TLS segment larger than one page is not supported yet!");
        }

        debug(LOADER,
              "Loader::prepareHeaders: TLS Segment requires %zu pages.\n",
              tls_pages);

        for (size_t i = 0; i < tls_pages; i++) {
          debug(LOADER,
                "Loader::prepareHeaders: Mapping TLS segment page %zu at vaddr "
                "%zx\n",
                i, ((*it).p_vaddr) + i * PAGE_SIZE);
          size_t ppn = FrameAlloc::instance()->allocPPN();
          size_t current_offset_in_segment = i * PAGE_SIZE;
          size_t vpn = ((*it).p_vaddr) + current_offset_in_segment;
          bool mapped = arch_memory_.mapPage(vpn, ppn, 0);
          if (!mapped) {
            debug(
                LOADER,
                "Loader::prepareHeaders: Failed to map TLS segment page %zu\n",
                i);
            return false;
          }

          // create the master thread local storage area
          void *phys_addr = (void *)ArchMemory::getIdentAddressOfPPN(ppn);

          if (readFromBinary(
                  (char *)phys_addr + current_offset_in_segment,
                  (*it).p_offset + current_offset_in_segment,
                  aostl::min((size_t)PAGE_SIZE,
                            (*it).p_filesz - current_offset_in_segment))) {
            debug(LOADER,
                  "Loader::prepareHeaders: Failed to load TLS segment data "
                  "from binary for page %zu\n",
                  i);
            return false;
          } else {
            debug(LOADER,
                  "Loader::prepareHeaders: Successfully loaded TLS segment "
                  "data from binary for page %zu\n",
                  i);

            // read size_t of initialized data and debug
            size_t init_data_size = aostl::min(
                (*it).p_filesz - current_offset_in_segment, (size_t)PAGE_SIZE);
            debug(LOADER,
                  "Loader::prepareHeaders: TLS segment page %zu: initialized "
                  "data size: %zu bytes\n",
                  i, init_data_size);

            // read size_t of unitialzed data and debug
            size_t uninit_data_size = aostl::min(
                (*it).p_memsz - (*it).p_filesz - current_offset_in_segment,
                (size_t)PAGE_SIZE - init_data_size);

            debug(LOADER,
                  "Loader::prepareHeaders: TLS segment page %zu: uninitialized "
                  "data size: %zu bytes\n",
                  i, uninit_data_size);

            if ((*it).p_memsz > (*it).p_filesz) {
              size_t zero_offset = (*it).p_filesz;
              size_t zero_length = (*it).p_memsz - (*it).p_filesz;
              memset((char *)phys_addr + zero_offset, 0, zero_length);
            }

            // set the tls_info_ structure
            tls_info_.ident_address = phys_addr;
            tls_info_.phdr.p_paddr = 0; // ignore ?
            tls_info_.phdr.p_vaddr = (*it).p_vaddr;
            tls_info_.phdr.p_memsz = (*it).p_memsz;
            tls_info_.phdr.p_filesz = (*it).p_filesz;
            tls_info_.phdr.p_offset = (*it).p_offset;
            tls_info_.phdr.p_align = (*it).p_align;
            tls_info_.phdr.p_flags = (*it).p_flags;
            tls_info_.phdr.p_type = (*it).p_type;
            tls_info_.found = true;

            // only allow first page for now
            break;
          }
        }

        break;
      }
    }
  }
  // END TLS SEGMENT HANDLING

  aostl::list<Elf::Phdr>::iterator it, it2;
  for (it = phdrs_.begin(); it != phdrs_.end(); it++) {
    // remove sections which shall not be load from anywhere
    if ((*it).p_type != Elf::PT_LOAD ||
        ((*it).p_memsz == 0 && (*it).p_filesz == 0)) {
      it = phdrs_.erase(it, 1) - 1;
      continue;
    }
    // check if some sections shall load data from the binary to the same
    // location
    for (it2 = phdrs_.begin(); it2 != it; it2++) {
      if (aostl::max((*it).p_vaddr, (*it2).p_vaddr) <
          aostl::min((*it).p_vaddr + (*it).p_filesz,
                    (*it2).p_vaddr + (*it2).p_filesz)) {
        debug(LOADER, "Loader::prepareHeaders: Failed to load the segments, "
                      "some of them overlap!\n");
        return false;
      }
    }
  }
  return phdrs_.size() > 0;
}
