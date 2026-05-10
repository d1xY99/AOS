#pragma once

#include "FsTypeReg.h"
#include "assert.h"

class MinixFsReg : public FsTypeReg
{
public:
    MinixFsReg() :
        FsTypeReg("minixfs")
    {

    }

    virtual ~MinixFsReg()
    {

    }

    virtual Superblock *readSuper(Superblock *, void *) const
    {
        assert(0 && "Not implemented in aos-mkimg");
    }

    virtual Superblock *createSuper(uint32)
    {
        assert(0 && "Not implemented in aos-mkimg");
    }
};
