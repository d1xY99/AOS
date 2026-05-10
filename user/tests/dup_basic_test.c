#include "unistd.h"
#include <stdio.h>
#include <pthread.h>

void *write_thread(void *arg) {
  int write_fd = *((int *)arg);

  char msg[68] = "Copy please?";
  write(write_fd, msg, sizeof(msg));
	return 0;
}

void *dup_read_thread(void *arg) {
  int read_fd = dup(*((int *)arg));

  char buf[68];

  read(read_fd, buf, sizeof(buf));
  printf("Read from pipe: %s\n", buf);

  close(read_fd);
	return 0;
}

int main() {
  int fd[2];
  pipe(fd);

  pthread_t writer_thread, reader_thread;
	printf("Creating threads...\n");
  pthread_create(&writer_thread, NULL, write_thread, &fd[0]);
  pthread_create(&reader_thread, NULL, dup_read_thread, &fd[1]);

	printf("Joining threads\n");
  pthread_join(writer_thread, NULL);
  pthread_join(reader_thread, NULL);

  printf("END\n");
  return 0;
}