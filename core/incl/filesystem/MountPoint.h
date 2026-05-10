#pragma once

#include "types.h"
#include "Vfs.h"

class Superblock;
class Dentry;

extern Vfs vfs;

// Mount flags
// Only MS_RDONLY is supported by now.

/**
 * Mount the Filesystem read-only
 */
#define MS_RDONLY 1

class MountPoint
{
 protected:

  /**
   * Points to the parent filesystem on which this filesystem is mounted on.
   */
  MountPoint *mnt_parent_;

  /**
   * Points to the Dentry of the mount directory of this filesystem.
   */
  Dentry *mnt_mountpoint_;

  /**
   * Points to the Dentry of the root directory of this filesystem.
   */
  Dentry *mnt_root_;

  /**
   * Points to the superblock object of this filesystem.
   */
  Superblock *mnt_sb_;

  /**
   * The mnt_flags_ field of the descriptor stores the value of several flags
   * that specify how some kinds of files in the mounted filesystem are
   * handled.
   */
  int32 mnt_flags_;

 public:
  MountPoint();


  /**
   * constructor
   * @param parent the parent dentry of the mount point
   * @param mountpoint the mount points dentry
   * @param root the root dentry
   * @param superblock the superblock mounted
   * @param flags the flags
   */
  MountPoint(MountPoint* parent, Dentry * mountpoint, Dentry* root,
      Superblock* superblock, int32 flags);

  virtual ~MountPoint();

  /**
   * get the parent-MountPoint of the MountPoint
   * @return the parent-MountPoint
   */
  MountPoint *getParent() const;

  /**
   * get the mount-point of the MountPoint
   * @return the mount point dentry
   */
  Dentry *getMountPoint() const;

  /**
   * get the ROOT-directory of the MountPoint
   * @return the root dentry
   */
  Dentry *getRoot() const;

  /**
   * get the superblock fo the MountPoint
   * @return the superblock
   */
  Superblock *getSuperblock() const;

  /**
   * get the flags
   * @return the flags
   */
  int32 getFlags() const;

  /**
   * NOTE: only used as workaround
   */
  void clear();


  bool isRootMount() const;

};



