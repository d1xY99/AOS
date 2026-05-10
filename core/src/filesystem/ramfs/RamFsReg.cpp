#include "filesystem/ramfs/RamFsReg.h"
#include "filesystem/ramfs/RamFsSuper.h"


RamFsReg::RamFsReg() : FsTypeReg("ramfs")
{
}


RamFsReg::~RamFsReg()
{}


Superblock *RamFsReg::readSuper(Superblock *superblock, void*) const
{
  return superblock;
}


Superblock *RamFsReg::createSuper (uint32 s_dev)
{
  Superblock *super = new RamFsSuper(this, s_dev);
  return super;
}
