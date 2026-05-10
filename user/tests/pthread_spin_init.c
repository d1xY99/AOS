#include <assert.h>
#include <pthread.h>

int main(int argc, char *argv[]) {
  pthread_spinlock_t lock;
  int ret;

  ret = pthread_spin_init(&lock, 0);
  assert(ret == 0 && "pthread_spin_init failed");

  ret = pthread_spin_destroy(&lock);
  assert(ret == 0 && "pthread_spin_destroy failed");

  return 0;
}
