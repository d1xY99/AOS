#pragma once

#include "types.h"
#include "aostl/uatomic.h"
#include "File.h"
class FileDescriptor;

/**
 * Wraps a kernel FileDescriptor with per-process reference tracking.
 * Keeps the underlying global descriptor alive until all processes close it.
 */
class FileSlot {
public:
  enum class FileType {
    NORMAL,
    PIPE,
    SHARED_MEMORY
  };

  explicit FileSlot(FileDescriptor *descriptor, FileType type_ = FileType::NORMAL, uint32_t permission_ = O_RDWR);
  ~FileSlot();

  void incref();
  uint32 decref(); // returns remaining references after decrement

  FileDescriptor *getKernelDescriptor() const { return descriptor_; }
  FileType getType() const { return type_; }
  uint32_t getPermission() const { return permission_; }

private:
  FileDescriptor *descriptor_;
  aostl::atomic<uint32> ref_count_;
  FileType type_;
  uint32_t permission_;
};
