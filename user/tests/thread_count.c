#include "pthread.h"
#include "stdio.h"
int main(int argc, char *argv[]) {
  printf("Threadcount: %d", get_thread_count());
  return 0;
}
