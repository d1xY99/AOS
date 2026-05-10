#include "unistd.h"
#include "sys/syscall.h"
#include "../../../core/incl/kernel/syscall-definitions.h"

/**
 * Writes to a file descriptor.
 * Up to count bytes from the provided buffer are written to the given file.
 *
 * If the given file is capable of seeking, the write will start at the file
 * position associated with the descriptor. This offset is incremented by
 * the number of bytes actually written.
 *
 * @param file_descriptor file descriptor referencing the file to write
 * @param buffer the buffer where the write data will be placed
 * @param count the number of bytes to write
 * @param offset the absolute offset where the write operation starts
 * @return the number of bytes written on success, 0 if count is zero or\
 nothing was written, and -1 if an error occured
 *
 */
ssize_t write(int file_descriptor, const void *buffer, size_t count)
{
  return __syscall(sc_write, file_descriptor, (long) buffer, count, 0x00,
                   0x00);
}
