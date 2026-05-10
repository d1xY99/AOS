#pragma once

#include <ulist.h>

class BlockReq;
class BlockDev;

class BlockDevTable
{
  public:
    BlockDevTable();
    ~BlockDevTable();

    /**
     * returns singleton instance
     * @return the block device manager instance
     */
    static BlockDevTable *getInstance();

    /**
     * detects all devices present
     */
    void doDeviceDetection();

    /**
     * adds the given device to the manager
     * @param dev the device to add
     */
    void addVirtualDevice(BlockDev *dev);

    /**
     * returns the device with the given number
     * @param dev_num the device number
     * @return the device
     */
    BlockDev *getDeviceByNumber(uint32 dev_num);

    /**
     * returns the device with the given name
     * @param dev_name the device name
     * @return the device
     */
    BlockDev *getDeviceByName(const char *dev_name);

    /**
     * returns the number of devices in the bd manager
     * @return the number of devices
     */
    uint32 getNumberOfDevices();

    /**
     * adds the given request to the device given in the request
     * @param bdr the request
     */
    void addRequest(BlockReq *bdr);

    /**
     * calls seviceIRQ on the device the irq with the given number is on
     * after that probeIRQ is false
     * @param irq_num the irq number
     */
    void serviceIRQ(uint32 irq_num);

    /**
     * gets false when the irq is serviced
     */
    bool probeIRQ;

    aostl::list<BlockDev *> device_list_;

  protected:
    static BlockDevTable *instance_;
};


