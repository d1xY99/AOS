#include "filesystem/ramfs/RamFsNode.h"
#include "kstring.h"
#include "assert.h"
#include "filesystem/ramfs/RamFsSuper.h"
#include "filesystem/ramfs/RamFsFileObj.h"
#include "filesystem/Dentry.h"
#include "FsTypeReg.h"

#include "display/kprintf.h"

#define BASIC_ALLOC 256

RamFsNode::RamFsNode(Superblock *super_block, uint32 inode_type) :
    Inode(super_block, inode_type),
    data_(0)
{
  debug(RAMFS, "New RamFsNode %p\n", this);
  if (inode_type == I_FILE)
  {
    data_ = new char[BASIC_ALLOC]();
    i_size_ = BASIC_ALLOC;
  }
}

RamFsNode::~RamFsNode()
{
  debug(RAMFS, "Destroying RamFsNode %p\n", this);
  delete[] data_;
}

int32 RamFsNode::readData(uint32 offset, uint32 size, char *buffer)
{
  if(offset >= getSize())
  {
    return 0;
  }

  uint32 read_size = Min(size, getSize() - offset);

  char *ptr_offset = data_ + offset;
  memcpy(buffer, ptr_offset, read_size);
  return read_size;
}

int32 RamFsNode::writeData(uint32 offset, uint32 size, const char *buffer)
{
  assert(i_type_ == I_FILE);

  if(offset >= getSize())
  {
    return 0;
  }

  uint32 write_size = Min(size, getSize() - offset);
  if(write_size != size)
  {
    debug(RAMFS, "WARNING: RamFS currently does not support expanding files via the write syscall\n");
  }

  char *ptr_offset = data_ + offset;
  memcpy(ptr_offset, buffer, write_size);
  return write_size;
}

int32 RamFsNode::truncate(size_t length)
{
  assert(i_type_ == I_FILE);

  const size_t max_size = static_cast<size_t>(static_cast<uint32>(-1));
  if (length > max_size)
  {
    return -1;
  }

  if (length == i_size_)
  {
    return 0;
  }

  char *new_data = nullptr;
  if (length > 0)
  {
    new_data = new char[length]();
    size_t copy_len =
        length < i_size_ ? length : static_cast<size_t>(i_size_);
    if (data_ && copy_len > 0)
    {
      memcpy(new_data, data_, copy_len);
    }
  }

  delete[] data_;
  data_ = new_data;
  i_size_ = static_cast<uint32>(length);
  return 0;
}

File* RamFsNode::open(Dentry* dentry, uint32 flag)
{
  debug(INODE, "%s Inode: Open file\n", getSuperblock()->getFSType()->getFSName());
  assert(aostl::find(i_dentrys_.begin(), i_dentrys_.end(), dentry) != i_dentrys_.end());

  File* file = (File*) (new RamFsFileObj(this, dentry, flag));
  i_files_.push_back(file);
  getSuperblock()->fileOpened(file);
  return file;
}
