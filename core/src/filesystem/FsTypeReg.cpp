#include "FsTypeReg.h"
#include "assert.h"

FsTypeReg::FsTypeReg(const char *fs_name) :
    fs_name_ ( fs_name ),
    fs_flags_ ( 0 )
{}


FsTypeReg::~FsTypeReg()
{}


const char* FsTypeReg::getFSName() const
{
  return fs_name_;
}


int32 FsTypeReg::getFSFlags() const
{
  return fs_flags_;
}
