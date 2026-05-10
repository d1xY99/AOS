#include "assert.h"
#include "pthread.h"
#include "sched.h"

struct slot
{
  volatile int tid;
};

static void *record_tid(void *param)
{
  struct slot *entry = (struct slot *)param;
  entry->tid = pthread_self();
  sched_yield();
  return NULL;
}

int create_wave_stress(void)
{
  const int waves = 3;
  const int per_wave = 8;
  pthread_t ids[per_wave];
  struct slot slots[per_wave];
  pthread_t history[waves * per_wave];
  int history_count = 0;

  for (int wave = 0; wave < waves; ++wave)
  {
    for (int i = 0; i < per_wave; ++i)
    {
      slots[i].tid = -1;
      int rv = pthread_create(&ids[i], NULL, record_tid, &slots[i]);
      assert(rv == 0);
    }

    for (int i = 0; i < per_wave; ++i)
    {
      int rv = pthread_join(ids[i], NULL);
      assert(rv == 0);

      assert(ids[i] != 0);
      for (int j = 0; j < i; ++j)
      {
        assert(ids[i] != ids[j]);
      }

      assert(slots[i].tid != -1);
      history[history_count++] = ids[i];
    }
  }

  for (int i = 0; i < history_count; ++i)
  {
    assert(history[i] != 0);
  }

  return 0;
}
