#include "FileSlotTable.h"

#include "FileDescriptor.h"
#include "FileSlot.h"
#include "ScopeLock.h"
#include "Pipe.h"
#include "assert.h"
#include "uvector.h"
#include "debug.h"

extern FileDescriptorList global_fd_list;

FileSlotTable::FileSlotTable()
    : lock_("FileSlotTable::lock"), table_() {}

FileSlotTable::~FileSlotTable() { closeAll(); }

bool FileSlotTable::registerDescriptor(int32 fd)
{
  if (fd < 0)
    return false;

  FileDescriptor *kernel_fd =
      global_fd_list.getFileDescriptor(static_cast<uint32>(fd));
  if (!kernel_fd)
    return false;

  ScopeLock lock(lock_);
  auto it = table_.find(static_cast<uint32>(fd));
  if (it != table_.end())
  {
    it->second->incref();
    return true;
  }

  FileSlot *local = new FileSlot(kernel_fd);
  local->incref();
  table_.insert(aostl::pair<uint32, FileSlot *>(
      static_cast<uint32>(fd), local));
  debug(PIPE,
        "FileSlotTable::registerDescriptor: fd=%d kernel fd=%u ptr=%p "
        "into table %p\n",
        fd, kernel_fd->getFd(), kernel_fd, this);
  return true;
}

bool FileSlotTable::closeDescriptor(int32 fd)
{
  if (fd < 0)
    return false;

  FileSlot *local = nullptr;
  {
    ScopeLock lock(lock_);
    auto it = table_.find(static_cast<uint32>(fd));
    if (it == table_.end())
      return false;
    local = it->second;
    table_.erase(it);
  }

  if (local->decref() == 0)
    delete local;

  return true;
}

FileSlot *FileSlotTable::getLocalDescriptor(int32 fd)
{
  if (fd < 0)
    return nullptr;

  ScopeLock lock(lock_);
  auto it = table_.find(static_cast<uint32>(fd));
  if (it == table_.end())
    return nullptr;
  return it->second;
}

FileDescriptor *FileSlotTable::getKernelDescriptor(int32 fd) const
{
  if (fd < 0)
    return nullptr;

  ScopeLock lock(lock_);
  auto it = table_.find(static_cast<uint32>(fd));
  if (it == table_.end())
    return nullptr;
  return it->second->getKernelDescriptor();
}

int FileSlotTable::findFdByKernelDescriptor(
    FileDescriptor *descriptor) const
{
  if (!descriptor)
    return -1;

  ScopeLock lock(lock_);
  for (auto &entry : table_)
  {
    FileSlot *local = entry.second;
    if (!local)
      continue;
    if (local->getKernelDescriptor() == descriptor)
      return static_cast<int>(entry.first);
  }
  return -1;
}

void FileSlotTable::cloneFrom(
    const FileSlotTable &other)
{
  if (&other == this)
    return;

  ScopeLock other_lock(other.lock_);
  ScopeLock this_lock(lock_);

  for (const auto &entry : other.table_)
  {
    if (table_.find(entry.first) != table_.end())
      continue;
    FileSlot *source = entry.second;
    if (!source)
      continue;
    FileDescriptor *kernel_descriptor = source->getKernelDescriptor();
    if (!kernel_descriptor)
      continue;
    debug(PIPE,
          "FileSlotTable::cloneFrom: inheriting kernel fd=%u ptr=%p "
          "into table %p\n",
          kernel_descriptor->getFd(), kernel_descriptor, this);

    FileSlot *local = new FileSlot(
        kernel_descriptor, source->getType(), source->getPermission());
    local->incref();
    table_.insert(aostl::pair<uint32, FileSlot *>(
        entry.first, local));
  }
}

void FileSlotTable::closeAll()
{
  aostl::vector<FileSlot *> descriptors;
  {
    ScopeLock lock(lock_);
    descriptors.reserve(table_.size());
    for (auto &entry : table_)
      descriptors.push_back(entry.second);
    table_.clear();
  }

  for (FileSlot *descriptor : descriptors)
  {
    if (descriptor->decref() == 0)
      delete descriptor;
  }
}

void FileSlotTable::shutdownPipes()
{
  aostl::vector<Pipe *> pipes;
  {
    ScopeLock lock(lock_);
    pipes.reserve(table_.size());
    for (auto &entry : table_)
    {
      FileSlot *local = entry.second;
      if (!local || local->getType() != FileSlot::FileType::PIPE)
        continue;
      FileDescriptor *descriptor = local->getKernelDescriptor();
      if (!descriptor)
        continue;
      pipes.push_back(static_cast<Pipe *>(descriptor));
    }
  }

  for (Pipe *pipe : pipes)
  {
    pipe->shutdown();
  }
}

size_t FileSlotTable::createLocalDescriptor(FileDescriptor *descriptor, FileSlot::FileType type_, uint32_t mod_)
{
  assert(descriptor != nullptr);

  ScopeLock lock(lock_);

  debug(PIPE,
        "FileSlotTable::createLocalDescriptor: kernel fd=%u ptr=%p "
        "in table %p\n",
        descriptor->getFd(), descriptor, this);

  uint32 new_fd_id = 3;
  while (table_.find(new_fd_id) != table_.end())
  {
    ++new_fd_id;
  }

  FileSlot *local_fd = new FileSlot(descriptor, type_, mod_);
  local_fd->incref();
  // table_[new_fd_id] = local_fd;
  table_.insert(aostl::pair<uint32, FileSlot *>(static_cast<uint32>(new_fd_id), local_fd));

  return new_fd_id;
}

bool FileSlotTable::assignDescriptor(
    int32 fd, FileDescriptor *descriptor, FileSlot::FileType type_,
    uint32_t mod_)
{
  assert(descriptor != nullptr);
  if (fd < 0)
    return false;

  ScopeLock lock(lock_);
  if (table_.find(static_cast<uint32>(fd)) != table_.end())
    return false;

  debug(PIPE,
        "FileSlotTable::assignDescriptor: fd=%d kernel fd=%u ptr=%p "
        "table %p\n",
        fd, descriptor->getFd(), descriptor, this);

  FileSlot *local_fd =
      new FileSlot(descriptor, type_, mod_);
  local_fd->incref();
  table_.insert(aostl::pair<uint32, FileSlot *>(
      static_cast<uint32>(fd), local_fd));

  return true;
}
