// #include <pthread.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include "assert.h"

// /* ------------------------------- Testing 300 pthread_create -------------------------------*/

// void *exit_function(void *arg)
// {
//     pthread_exit(arg);
//     return NULL;
// }
// void *thread_function(void *arg)
// {
//     pthread_t th;
//     int ret = pthread_create(&th, NULL, exit_function, arg);
//     return NULL;
// }

// int main(void)
// {
//     size_t i = 0;
//     while (1)
//     {
//         pthread_t threads;
//         int ret = pthread_create(&threads, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");

//         pthread_t th2;
//         int ret2 = pthread_create(&th2, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");

//         pthread_t th3;
//         int ret3 = pthread_create(&th3, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");

//         pthread_t th4;
//         int ret4 = pthread_create(&th4, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");

//         pthread_t th5;
//         int ret5 = pthread_create(&th5, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");

//         pthread_t th6;
//         int ret6 = pthread_create(&th6, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");

//         pthread_t th7;
//         int ret7 = pthread_create(&th7, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");

//         pthread_t th8;
//         int ret8 = pthread_create(&th8, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");

//         pthread_t th9;
//         int ret9 = pthread_create(&th9, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");

//         pthread_t th10;
//         int ret10 = pthread_create(&th10, NULL, thread_function, NULL);
//         assert(ret == 0 && "pthread_create failed\n");
//     }
//     return 0;
// }

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define NUM_LAUNCHERS 10 // number of looping threads
#define SLEEP_BETWEEN 0  // set to small value (like 1) to slow down creation

// simple thread that does nothing and exits immediately
void *worker_thread(void *arg)
{
    pthread_exit(NULL);
    return NULL;
}

// launcher threads: endlessly create short-lived threads
void *launcher_thread(void *arg)
{
    long id = (long)arg;
    printf("[Launcher %ld] started.\n", id);

    while (1)
    {
        pthread_t tid;
        int rv = pthread_create(&tid, NULL, worker_thread, NULL);
        if (rv != 0)
        {
            printf("[Launcher %ld] pthread_create failed: %d\n", id, rv);
            // optional: break if you want to stop on error
            continue;
        }

        // // join immediately to recycle resources
        // rv = pthread_join(tid, NULL);
        // if (rv != 0)
        // {
        //     printf("[Launcher %ld] pthread_join failed: %d\n", id, rv);
        // }

        // optional sleep to prevent CPU starvation
        // if (SLEEP_BETWEEN > 0)
        //     usleep(SLEEP_BETWEEN);
    }

    return NULL;
}

int main(void)
{
    pthread_t launchers[NUM_LAUNCHERS];

    printf("=== Thread Stress Test: %d launcher threads ===\n", NUM_LAUNCHERS);

    for (long i = 0; i < NUM_LAUNCHERS; i++)
    {
        int rv = pthread_create(&launchers[i], NULL, launcher_thread, (void *)i);
        assert(rv == 0);
    }

    // Main thread just waits forever while launcher threads run
    while (1)
    {
        sleep(1);
    }

    return 0;
}
