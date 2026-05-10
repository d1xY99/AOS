#pragma once

#include "types.h"
class BlockReq;

class BlockDevDriver
{
  public:

    virtual ~BlockDevDriver()
    {
    }

    virtual uint32 addRequest(BlockReq *) = 0;

    virtual int32 readSector(uint32, uint32, void *) = 0;

    virtual int32 writeSector(uint32, uint32, void *) = 0;

    virtual uint32 getNumSectors() = 0;

    virtual uint32 getSectorSize() = 0;

    virtual void serviceIRQ() = 0;

    uint16 irq;
};


