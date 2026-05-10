#include "filesystem/devicefs/DevFsReg.h"
#include "filesystem/devicefs/DevFsSuper.h"

DevFsReg* DevFsReg::instance_ = nullptr;

DevFsReg::DevFsReg() :
    RamFsReg()
{
    fs_name_ = "devicefs";
}

DevFsReg::~DevFsReg()
{
}

Superblock* DevFsReg::readSuper(Superblock *superblock, void*) const
{
  return superblock;
}

Superblock* DevFsReg::createSuper(uint32 __attribute__((unused)) s_dev)
{
  Superblock *super = DevFsSuper::getInstance();
  return super;
}

DevFsReg* DevFsReg::getInstance()
{
    if (!instance_)
        instance_ = new DevFsReg();
    return instance_;
}
