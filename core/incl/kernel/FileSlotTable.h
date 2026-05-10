#pragma once

#include "Mutex.h"
#include "types.h"
#include "umap.h"
#include "FileSlot.h"

class FileSlot;
class FileDescriptor;

class FileSlotTable {
public:
  FileSlotTable();
  ~FileSlotTable();

  bool registerDescriptor(int32 fd);
  bool closeDescriptor(int32 fd);
  FileSlot *getLocalDescriptor(int32 fd);
  FileDescriptor *getKernelDescriptor(int32 fd) const;
  int findFdByKernelDescriptor(FileDescriptor *descriptor) const;
  void cloneFrom(const FileSlotTable &other);
  void closeAll();
  void shutdownPipes();
  size_t createLocalDescriptor(FileDescriptor *descriptor, FileSlot::FileType type_, uint32_t mod_);
  bool assignDescriptor(int32 fd, FileDescriptor *descriptor,
                        FileSlot::FileType type_, uint32_t mod_);

private:
  mutable Mutex lock_;
  aostl::map<uint32, FileSlot *> table_;
};
