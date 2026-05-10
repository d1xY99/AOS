#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int sleep_return_test(void)
{
    const unsigned durations[] = {1, 2};
    clock_t last_clock = clock();

    for (unsigned i = 0; i < sizeof(durations) / sizeof(durations[0]); ++i)
    {
        unsigned sec = durations[i];
        printf("[sleep_return_test] sleeping for %u second(s)\n", sec);
        int remaining = sleep(sec);
        if (remaining != 0)
        {
            printf("[sleep_return_test] ERROR: sleep(%u) returned %d\n", sec, remaining);
            return 1;
        }

        clock_t now = clock();
        if (now < last_clock)
        {
            printf("[sleep_return_test] ERROR: clock decreased (prev=%ld now=%ld)\n",
                   (long)last_clock, (long)now);
            return 1;
        }
        last_clock = now;
    }

    printf("[sleep_return_test] sleep() completed all intervals\n");
    return 0;
}