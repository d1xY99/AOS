#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

/*
   ===========================================
   DEADLOCK DEMO (Circular Thread Joining)
   ===========================================
   - Four threads are created: A, B, C, D.
   - Each thread waits for the next one to end.
   - D waits for A, forming a circular wait.
   - atleast one thread should receive EDEADLK error.
   - The program asserts the expected behavior.
*/

pthread_t threadA;
pthread_t threadB;
pthread_t threadC;
pthread_t threadD;

// ------------------------
// Thread A waits for B
// ------------------------
void *taskA(void *arg)
{
  for (int i = 0; i < 20000; i++)
  {
    // do some dummy work
  }

  int result = pthread_join(threadB, NULL);
  assert(result == 0 && "Expected 0 from joining thread B");
  return NULL;
}

// ------------------------
// Thread B waits for C
// ------------------------
void *taskB(void *arg)
{
  for (int i = 0; i < 20000; i++)
  {
    // simulate some processing
  }

  int result = pthread_join(threadC, NULL);
  assert(result == 0 && "Expected 0 from joining thread C");
  return NULL;
}

// ------------------------
// Thread C waits for D
// ------------------------
void *taskC(void *arg)
{
  for (int i = 0; i < 20000; i++)
  {
    // simulate work
  }

  int result = pthread_join(threadD, NULL);
  assert(result == 0 && "Expected 0 from joining thread D");
  return NULL;
}

// ------------------------
// Thread D waits for A (creates the loop)
// ------------------------
void *taskD(void *arg)
{
  for (int i = 0; i < 20000; i++)
  {
    // simulate work
  }

  int result = pthread_join(threadA, NULL);
  assert(result == 35 && "Expected EDEADLK from joining thread A");
  return NULL;
}

// ------------------------
// Main: creates threads and runs the demo
// ------------------------
int main(void)
{
  // Create the four threads
  int r1 = pthread_create(&threadA, NULL, taskA, NULL);
  int r2 = pthread_create(&threadB, NULL, taskB, NULL);
  int r3 = pthread_create(&threadC, NULL, taskC, NULL);
  int r4 = pthread_create(&threadD, NULL, taskD, NULL);

  // Verify all threads were created successfully
  assert(r1 == 0 && r2 == 0 && r3 == 0 && r4 == 0);

  // Let them run and reach the deadlock point
  sleep(2);

  printf("at least one thread should have detected a deadlock cycle.\n");
  return 0;
}
