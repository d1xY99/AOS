#include "MinixFsReg.h"
#include "MinixSuper.h"
#include "BlockDevTable.h"
#include "BlockDev.h"

MinixFsReg::MinixFsReg() : FsTypeReg("minixfs")
{
    fs_flags_ |= FS_REQUIRES_DEV;
}


MinixFsReg::~MinixFsReg()
{
}


Superblock *MinixFsReg::readSuper(Superblock *superblock, void*) const
{
  return superblock;
}


Superblock *MinixFsReg::createSuper(uint32 s_dev)
{
  if (s_dev == (uint32) -1)
    return 0;

  BlockDevTable::getInstance()->getDeviceByNumber(s_dev)->setBlockSize(BLOCK_SIZE);
  Superblock *super = new MinixSuper(this, s_dev, 0);
  return super;
}
