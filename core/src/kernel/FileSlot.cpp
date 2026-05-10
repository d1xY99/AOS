#include "FileSlot.h"

#include "FileDescriptor.h"
#include "File.h"
#include "Pipe.h"
#include "assert.h"

extern FileDescriptorList global_fd_list;

FileSlot::FileSlot(FileDescriptor *descriptor, FileType type_, uint32_t permission_)
    : descriptor_(descriptor), ref_count_(0), type_(type_), permission_(permission_) {
  assert(descriptor_);
  descriptor_->incref();
}

FileSlot::~FileSlot() {
  // No implicit release. Caller must ensure refcount reached zero and the
  // descriptor has been removed from global bookkeeping.
  descriptor_ = nullptr;
}

void FileSlot::incref() { ref_count_.fetch_add(1); }

uint32 FileSlot::decref() {
  uint32 previous = ref_count_.fetch_sub(1);
  assert(previous > 0 && "FileSlot::decref underflow");
  if (previous == 1) {
    assert(descriptor_ && "FileSlot::decref: descriptor_ null");
    bool last_descriptor = descriptor_->decref();
    if (last_descriptor) {
      if (type_ == FileType::PIPE) {
        Pipe *pipe = static_cast<Pipe *>(descriptor_);
        pipe->close();
      }
      assert(!global_fd_list.remove(descriptor_));
      if (File *file = descriptor_->getFile()) {
        file->closeFd(descriptor_);
      } else {
        delete descriptor_;
      }
    }
    descriptor_ = nullptr;
  }
  return previous - 1;
}
