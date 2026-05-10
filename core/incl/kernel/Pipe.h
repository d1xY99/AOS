#ifndef PIPE_H_
#define PIPE_H_

#include "FileDescriptor.h"
#include "Mutex.h"
#include "Condition.h"
#include "aostl/uatomic.h"

#define PIPE_BUF 4096

class PipeBuffer;

class Pipe : public FileDescriptor
{
public:
  enum class EndType
  {
    READ_END,
    WRITE_END
  };

  Pipe(File *file, PipeBuffer *buffer, EndType end_type);
  virtual ~Pipe();

  virtual size_t write(const char *buffer, uint32 size);
  virtual size_t read(char *buffer, uint32 count);
  virtual void close();
  virtual void shutdown();

private:
  PipeBuffer *buffer_;
  EndType end_type_;
  bool closed_;
};

class PipeBuffer
{
public:
  PipeBuffer();
  ~PipeBuffer() = default;

  void incref();
  void decref();

  void endpointOpened(Pipe::EndType end_type);
  void endpointClosed(Pipe::EndType end_type);
  size_t writeData(const char *buffer, uint32 size);
  size_t readData(char *buffer, uint32 count);
  void shutdown();

private:
  char internal_buffer_[PIPE_BUF];
  size_t head_;
  size_t tail_;
  size_t data_size_;
  bool shutdown_;
  size_t reader_count_;
  size_t writer_count_;
  aostl::atomic<uint32> ref_count_;
  Mutex mutex_;
  Condition not_empty_;
  Condition not_full_;
};

#endif
