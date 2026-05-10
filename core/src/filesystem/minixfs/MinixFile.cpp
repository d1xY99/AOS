#include "MinixFile.h"
#include "MinixNode.h"
#include "Inode.h"

MinixFile::MinixFile(Inode* inode, Dentry* dentry, uint32 flag) :
    File(inode, dentry, flag)
{
  f_superblock_ = inode->getSuperblock();
  offset_ = 0;
}

MinixFile::~MinixFile()
{
}

int32 MinixFile::read(char *buffer, size_t count, l_off_t offset)
{
  if (((flag_ & O_RDONLY) || (flag_ & O_RDWR)) && (f_inode_->getMode() & A_READABLE))
  {
    int32 read_bytes = f_inode_->readData(offset_ + offset, count, buffer);
    offset_ += read_bytes;
    return read_bytes;
  }
  else
  {
    // ERROR_FF
    return -1;
  }
}

int32 MinixFile::write(const char *buffer, size_t count, l_off_t offset)
{
  if (((flag_ & O_WRONLY) || (flag_ & O_RDWR)) && (f_inode_->getMode() & A_WRITABLE))
  {
    int32 written = f_inode_->writeData(offset_ + offset, count, buffer);
    offset_ += written;
    return written;
  }
  else
  {
    // ERROR_FF
    return -1;
  }
}

int32 MinixFile::flush()
{
  ((MinixNode *) f_inode_)->flush();
  return 0;
}

