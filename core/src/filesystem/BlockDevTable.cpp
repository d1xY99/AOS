#include "BlockDevDriver.h"
#include "BlockDevTable.h"
#include "BlockReq.h"
#include "BlockDev.h"
#include "IDEDriver.h"
#include "kprintf.h"
#include "debug.h"
#include "kstring.h"

BlockDevTable *BlockDevTable::getInstance()
{
  if (!instance_)
    instance_ = new BlockDevTable();
  return instance_;
}


BlockDevTable::BlockDevTable() :
    probeIRQ(false)
{
}

void BlockDevTable::doDeviceDetection(void)
{
  debug(BD_MANAGER, "doDeviceDetection: Detecting BD devices\n");
  IDEDriver id;
  // insert other device detectors here
  debug(BD_MANAGER, "doDeviceDetection:Detection done\n");
}

void BlockDevTable::addRequest(BlockReq* bdr)
{
  if (bdr->getDevID() < getNumberOfDevices())
    getDeviceByNumber(bdr->getDevID())->addRequest(bdr);
  else
    bdr->setStatus(BlockReq::BD_ERROR);
}

void BlockDevTable::addVirtualDevice(BlockDev* dev)
{
  debug(BD_MANAGER, "addVirtualDevice:Adding device\n");
  dev->setDeviceNumber(device_list_.size());
  device_list_.push_back(dev);
  debug(BD_MANAGER, "addVirtualDevice:Device added\n");
}

void BlockDevTable::serviceIRQ(uint32 irq_num)
{
  debug(BD_MANAGER, "serviceIRQ:Servicing IRQ\n");
  probeIRQ = false;

  for (BlockDev* dev : device_list_)
    if (dev->getDriver()->irq == irq_num)
    {
      dev->getDriver()->serviceIRQ();
      return;
    }

  debug(BD_MANAGER, "serviceIRQ:End servicing IRQ\n");
}

BlockDev* BlockDevTable::getDeviceByNumber(uint32 dev_num)
{
  return device_list_[dev_num];
}

BlockDev* BlockDevTable::getDeviceByName(const char * dev_name)
{
  if(!dev_name)
  {
      return 0;
  }

  debug(BD_MANAGER, "getDeviceByName: %s", dev_name);
  for (BlockDev* dev : device_list_)
  {
    if (strcmp(dev->getName(), dev_name) == 0)
    {
      debug(BD_MANAGER, "getDeviceByName: %s with id: %d\n", dev->getName(), dev->getDeviceNumber());
      return dev;
    }
  }
  return 0;
}

uint32 BlockDevTable::getNumberOfDevices(void)
{
  return device_list_.size();
}

BlockDevTable* BlockDevTable::instance_ = 0;
