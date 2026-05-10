#include "filesystem/devicefs/DevFsSuper.h"
#include "filesystem/ramfs/RamFsNode.h"
#include "DevFsReg.h"
#include "filesystem/Dentry.h"
#include "filesystem/Inode.h"
#include "filesystem/File.h"
#include "filesystem/FileDescriptor.h"

#include "display/kprintf.h"

#include "Console.h"

class DevFsReg;

extern Console* main_console;

const char DevFsSuper::ROOT_NAME[] = "/";
const char DevFsSuper::DEVICE_ROOT_NAME[] = "dev";

DevFsSuper* DevFsSuper::instance_ = 0;

DevFsSuper::DevFsSuper(DevFsReg* fs_type, uint32 s_dev) :
    RamFsSuper(fs_type, s_dev)
{
}

DevFsSuper::~DevFsSuper()
{
}

void DevFsSuper::addDevice(Inode* device_inode, const char* node_name)
{
  // Devices are mounted at the devicefs root (s_root_)
  device_inode->setSuperblock(this);

  Dentry* fdntr = new Dentry(device_inode, s_root_, node_name);

  assert(device_inode->mknod(fdntr) == 0);
  all_inodes_.push_back(device_inode);
}

DevFsSuper* DevFsSuper::getInstance()
{
    if (!instance_)
        instance_ = new DevFsSuper(DevFsReg::getInstance(), 0);
    return instance_;
}
