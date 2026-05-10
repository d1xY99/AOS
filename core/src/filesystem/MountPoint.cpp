#include "MountPoint.h"


MountPoint::MountPoint() :
    mnt_parent_ ( 0 ),
    mnt_mountpoint_ ( 0 ),
    mnt_root_ ( 0 ),
    mnt_sb_ ( 0 ),
    mnt_flags_ ( 0 )
{}


MountPoint::MountPoint ( MountPoint* parent, Dentry * mountpoint, Dentry* root,
                     Superblock* superblock, int32 flags ) :
    mnt_parent_ ( parent ? parent : this),
    mnt_mountpoint_ ( mountpoint ),
    mnt_root_ ( root ),
    mnt_sb_ ( superblock ),
    mnt_flags_ ( flags )
{}


MountPoint::~MountPoint()
{
  mnt_parent_ = 0;
  mnt_mountpoint_ = 0;
  mnt_root_ = 0;
  mnt_sb_ = 0;
  mnt_flags_ = 0;
}


MountPoint *MountPoint::getParent() const
{
  return mnt_parent_;
}


Dentry *MountPoint::getMountPoint() const
{
  return mnt_mountpoint_;
}


Dentry *MountPoint::getRoot() const
{
  return mnt_root_;
}


Superblock *MountPoint::getSuperblock() const
{
  return mnt_sb_;
}


int32 MountPoint::getFlags() const
{
  return mnt_flags_;
}


//NOTE: only used as workaround
void MountPoint::clear()
{
  mnt_parent_ = 0;
  mnt_mountpoint_ = 0;
  mnt_root_ = 0;
  mnt_sb_ = 0;
  mnt_flags_ = 0;
}


bool MountPoint::isRootMount() const
{
    return getParent() == this;
}
