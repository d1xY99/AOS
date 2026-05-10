#include "ZoneAllocBase.h"

ZoneAllocBase::ZoneAllocBase(uint16 num_inodes, uint16 num_zones) :
    inode_bitmap_(num_inodes), zone_bitmap_(num_zones)
{
}

ZoneAllocBase::~ZoneAllocBase()
{
}

