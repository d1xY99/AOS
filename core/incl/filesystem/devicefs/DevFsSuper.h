#pragma once

#include "filesystem/Superblock.h"
#include "filesystem/ramfs/RamFsSuper.h"

class Inode;
class Superblock;
class CharacterDevice;
class DevFsReg;

class DevFsSuper : public RamFsSuper
{
  public:
    static const char ROOT_NAME[];
    static const char DEVICE_ROOT_NAME[];

    virtual ~DevFsSuper();

    /**
     * addsa new device to the superblock
     * @param inode the inode of the device to add
     * @param node_name the device name
     */
    void addDevice(Inode* inode, const char* node_name);

    /**
     * Access method to the singleton instance
     */
    static DevFsSuper* getInstance();

  private:

    /**
     * Constructor
     * @param s_root the root Dentry of the new Filesystem
     * @param s_dev the device number of the new Filesystem
     */
    DevFsSuper(DevFsReg* fs_type, uint32 s_dev);

  protected:
    static DevFsSuper* instance_;

};

