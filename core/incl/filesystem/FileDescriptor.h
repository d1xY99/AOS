#pragma once

#include "types.h"
#include "ulist.h"
#include "umap.h"
#include "Mutex.h"
#include "aostl/uatomic.h"

class File;
class FileDescriptor;
class FileDescriptorList;

class FileDescriptor
{
  protected:
    size_t fd_;
    File* file_;
    aostl::atomic<uint32> ref_count_;

  public:
    FileDescriptor ( File* file );
    virtual ~FileDescriptor();
    uint32 getFd() { return fd_; }
    File* getFile() { return file_; }
    void incref();
    bool decref(); // true when last reference dropped

    friend File;
};

class FileDescriptorList
{
public:
    FileDescriptorList();
    ~FileDescriptorList();

    int add(FileDescriptor* fd);
    int remove(FileDescriptor* fd);
    FileDescriptor* getFileDescriptor(uint32 fd);

private:
    aostl::list<FileDescriptor*> fds_;
    Mutex fd_lock_;
};

extern FileDescriptorList global_fd_list;
