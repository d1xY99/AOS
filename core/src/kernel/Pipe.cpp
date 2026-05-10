#include "Pipe.h"
#include "debug.h"
#include "kstring.h"
#include "assert.h"

namespace
{
constexpr const char *PIPE_MUTEX_NAME = "PipeBuffer::mutex";
constexpr const char *PIPE_NOT_EMPTY = "PipeBuffer::not_empty";
constexpr const char *PIPE_NOT_FULL = "PipeBuffer::not_full";
} // namespace

PipeBuffer::PipeBuffer()
    : head_(0), tail_(0), data_size_(0), shutdown_(false),
      reader_count_(0), writer_count_(0), ref_count_(0),
      mutex_(PIPE_MUTEX_NAME), not_empty_(&mutex_, PIPE_NOT_EMPTY),
      not_full_(&mutex_, PIPE_NOT_FULL)
{
  memset(internal_buffer_, 0, sizeof(internal_buffer_));
}

void PipeBuffer::incref()
{
  ref_count_.fetch_add(1);
}

void PipeBuffer::decref()
{
  uint32 previous = ref_count_.fetch_sub(1);
  assert(previous > 0 && "PipeBuffer::decref underflow");
  if (previous == 1)
  {
    delete this;
    return;
  }
}

void PipeBuffer::endpointOpened(Pipe::EndType end_type)
{
  mutex_.acquire();
  if (end_type == Pipe::EndType::READ_END)
  {
    reader_count_++;
  }
  else
  {
    writer_count_++;
  }
  mutex_.release();
}

void PipeBuffer::endpointClosed(Pipe::EndType end_type)
{
  mutex_.acquire();
  if (end_type == Pipe::EndType::READ_END)
  {
    if (reader_count_ > 0)
      reader_count_--;
    not_full_.broadcast();
  }
  else
  {
    if (writer_count_ > 0)
      writer_count_--;
    not_empty_.broadcast();
  }
  if (reader_count_ == 0 && writer_count_ == 0)
  {
    shutdown_ = true;
    not_empty_.broadcast();
    not_full_.broadcast();
  }
  mutex_.release();
}

size_t PipeBuffer::writeData(const char *buffer, uint32 size)
{
  if (size == 0)
    return 0;

  mutex_.acquire();
  size_t total_written = 0;

  while (total_written < size)
  {
    while (!shutdown_ && reader_count_ > 0 && data_size_ == PIPE_BUF)
    {
      not_full_.wait();
    }

    if (shutdown_ || reader_count_ == 0)
    {
      size_t result = total_written ? total_written : static_cast<size_t>(-1);
      mutex_.release();
      return result;
    }

    internal_buffer_[tail_] = buffer[total_written];
    tail_ = (tail_ + 1) % PIPE_BUF;
    data_size_++;
    total_written++;
    not_empty_.signal();
  }

  mutex_.release();
  return total_written;
}

size_t PipeBuffer::readData(char *buffer, uint32 count)
{
  if (count == 0)
    return 0;

  mutex_.acquire();
  size_t total_read = 0;

  while (total_read < count)
  {
    while (data_size_ == 0 && writer_count_ > 0 && !shutdown_)
    {
      not_empty_.wait();
    }

    if (data_size_ == 0)
    {
      mutex_.release();
      return total_read;
    }

    buffer[total_read] = internal_buffer_[head_];
    head_ = (head_ + 1) % PIPE_BUF;
    data_size_--;
    total_read++;
    not_full_.signal();
  }

  mutex_.release();
  return total_read;
}

void PipeBuffer::shutdown()
{
  mutex_.acquire();
  shutdown_ = true;
  not_empty_.broadcast();
  not_full_.broadcast();
  mutex_.release();
}

Pipe::Pipe(File *file, PipeBuffer *buffer, EndType end_type)
    : FileDescriptor(file), buffer_(buffer), end_type_(end_type),
      closed_(false)
{
  assert(buffer_);
  buffer_->incref();
  buffer_->endpointOpened(end_type_);
}

Pipe::~Pipe()
{
  close();
}

size_t Pipe::write(const char *buffer, uint32 size)
{
  if (!buffer_ || end_type_ != EndType::WRITE_END)
    return static_cast<size_t>(-1);
  return buffer_->writeData(buffer, size);
}

size_t Pipe::read(char *buffer, uint32 count)
{
  if (!buffer_ || end_type_ != EndType::READ_END)
    return static_cast<size_t>(-1);
  return buffer_->readData(buffer, count);
}

void Pipe::close()
{
  if (!buffer_ || closed_)
    return;
  closed_ = true;
  buffer_->endpointClosed(end_type_);
  buffer_->decref();
  buffer_ = nullptr;
}

void Pipe::shutdown()
{
  if (buffer_)
  {
    buffer_->shutdown();
  }
}
