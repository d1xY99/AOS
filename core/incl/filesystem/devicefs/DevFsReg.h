#pragma once

#include "filesystem/FsTypeReg.h"
#include "filesystem/ramfs/RamFsReg.h"

class DevFsReg : public RamFsReg
{
  public:
    DevFsReg();

    virtual ~DevFsReg();

    /**
     * Reads the superblock from the device.
     * @param superblock is the superblock to fill with data.
     * @param data is the data given to the mount system call.
     * @return is a pointer to the resulting superblock.
     */
    virtual Superblock *readSuper(Superblock *superblock, void *data) const;

    /**
     * Creates an Superblock object for the actual file system type.
     * @param root the root dentry of the new superblock
     * @param s_dev the device number of the new superblock
     * @return a pointer to the Superblock object
     */
    virtual Superblock *createSuper(uint32 s_dev);

    static DevFsReg* getInstance();

protected:
    static DevFsReg* instance_;
};

