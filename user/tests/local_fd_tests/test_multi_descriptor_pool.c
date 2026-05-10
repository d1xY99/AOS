#include "test_support.h"

#include "assert.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"

#define DATA_FILE_COUNT 5
#define SCRATCH 128

int scenario_multiple_descriptors(void)
{
  printf("[multi_descriptor_pool] seeding %d files\n", DATA_FILE_COUNT);
  int handles[DATA_FILE_COUNT];
  char file_name[] = "usr/data0.log";
  char text_seed[] = "draft-0\n";
  char text_update[] = "final-0\n";
  char verification_buffer[SCRATCH];

  /* Prepare each file with unique baseline so later reads are predictable. */
  for (int index = 0; index < DATA_FILE_COUNT; ++index)
  {
    file_name[4] = '0' + index;
    text_seed[6] = '0' + index;
    assert(reset_file_contents(file_name, text_seed) == 0);
  }

  /* Open every file and write an updated tag to confirm descriptor independence. */
  for (int index = 0; index < DATA_FILE_COUNT; ++index)
  {
    file_name[4] = '0' + index;
    handles[index] = open(file_name, O_RDWR);
    assert(handles[index] >= 0);

    text_update[6] = '0' + index;
    ssize_t bytes = write(handles[index], text_update,
                          strlen(text_update));
    assert(bytes == (ssize_t)strlen(text_update));
    printf("[multi_descriptor_pool] wrote '%s' to %s\n", text_update, file_name);
  }

  /* Close the descriptors to flush data. */
  for (int index = 0; index < DATA_FILE_COUNT; ++index)
  {
    assert(close(handles[index]) == 0);
  }

  /* Re-open each file and ensure the stored tag matches what we wrote. */
  for (int index = 0; index < DATA_FILE_COUNT; ++index)
  {
    file_name[4] = '0' + index;
    assert(read_file_into_buffer(file_name, verification_buffer,
                                 sizeof(verification_buffer)) == 0);

    text_update[6] = '0' + index;
    assert(strcmp(verification_buffer, text_update) == 0);
    printf("[multi_descriptor_pool] verified %s contains '%s'\n",
           file_name, text_update);
  }

  return 0;
}
