#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <sched.h>

/*
  Goal :
  - Launch many worker threads that all wait on a global "start" flag.
  - Once released, each worker creates a tiny helper thread that returns 1,
    joins it, adds 1, and returns (so each worker ultimately returns 2).
  - Main spawns workers, pauses briefly so they reach the wait loop,
    flips the start flag, then joins all workers.
*/

#define WORKER_COUNT 150
static volatile int start_gun_fired = 0;

// --- tiny helper -----------------------------------------------------------
// A minimal thread that simply returns (void*)1 to its joiner.
static void *tiny_helper()
{
    return (void *)(size_t)1;
}

// --- worker entry ----------------------------------------------------------
// Waits for the start flag, then runs the tiny helper and returns helper+1.
static void *worker_entry()
{

    // 1) Wait for all workers to be "armed" before proceeding.
    while (!start_gun_fired)
    {
        // Make space for others while we spin.
        sched_yield();
    }

    // 2) Create the tiny helper thread that will just return 1.
    pthread_t helper_tid;
    int rc = pthread_create(&helper_tid, NULL, tiny_helper, NULL);
    assert(rc == 0 && "pthread_create(tiny_helper) failed");

    // 3) Join the helper and check its return value.
    void *helper_ret = NULL;
    rc = pthread_join(helper_tid, &helper_ret);
    assert(rc == 0 && "pthread_join(tiny_helper) failed");
    assert(helper_ret == (void *)(size_t)1 &&
           "tiny_helper must return 1");

    // 4) Return helper_result + 1 => (2).
    size_t final_value = (size_t)helper_ret + 1;
    assert(final_value == 2 && "final worker result must be 2");
    return (void *)final_value;
}

int main(void)
{
    // Pre-flight checks
    assert(WORKER_COUNT > 0 && "must have at least one worker");
    pthread_t workers[WORKER_COUNT];

    // Step A: spawn all workers (they will block on the start flag).
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        int rc = pthread_create(&workers[i], NULL, worker_entry, NULL);
        assert(rc == 0 && "pthread_create(worker_entry) failed");
    }

    // Step B: give workers time to reach the waiting loop.
    // (Busy-wait loop kept from original; acts as a crude barrier.)
    for (volatile size_t spin = 0; spin < 200000000; ++spin)
    {
        // no-op
    }

    // Step C: release all workers at once.
    start_gun_fired = 1;

    // Step D: join all workers; each should return 2.
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        void *worker_ret = NULL;
        int rc = pthread_join(workers[i], &worker_ret);
        assert(rc == 0 && "pthread_join(worker) failed");
        assert(worker_ret == (void *)(size_t)2 &&
               "worker must return 2 (helper(1) + 1)");
    }

    // If we got here, all workers behaved as expected.
    return 0;
}
