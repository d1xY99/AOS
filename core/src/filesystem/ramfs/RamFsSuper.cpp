#include "filesystem/FileDescriptor.h"
#include "filesystem/ramfs/RamFsSuper.h"
#include "filesystem/ramfs/RamFsNode.h"
#include "filesystem/ramfs/RamFsFileObj.h"
#include "filesystem/ramfs/RamFsReg.h"
#include "filesystem/Dentry.h"
#include "assert.h"

#include "display/kprintf.h"
#include "display/debug.h"
#define ROOT_NAME "/"

RamFsSuper::RamFsSuper(RamFsReg* fs_type, uint32 s_dev) :
    Superblock(fs_type, s_dev)
{
  Inode *root_inode = createInode(I_DIR);
  s_root_ = new Dentry(root_inode);
  assert(root_inode->mkdir(s_root_) == 0);
}

RamFsSuper::~RamFsSuper()
{
  assert(dirty_inodes_.empty() == true);

  releaseAllOpenFiles();

  deleteAllInodes();
}

Inode* RamFsSuper::createInode(uint32 type)
{
    debug(RAMFS, "createInode, type: %x\n", type);
    auto inode = new RamFsNode(this, type);

    all_inodes_.push_back(inode);
    return inode;
}

int32 RamFsSuper::readInode(Inode* inode)
{
  assert(inode);

  if (aostl::find(all_inodes_, inode) == all_inodes_.end())
  {
    all_inodes_.push_back(inode);
  }
  return 0;
}

void RamFsSuper::writeInode(Inode* inode)
{
  assert(inode);

  if (aostl::find(all_inodes_, inode) == all_inodes_.end())
  {
    all_inodes_.push_back(inode);
  }
}

void RamFsSuper::deleteInode(Inode* inode)
{
  all_inodes_.remove(inode);
  delete inode;
}
