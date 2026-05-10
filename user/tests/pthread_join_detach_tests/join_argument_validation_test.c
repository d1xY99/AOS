#include "pthread.h"
#include "stdio.h"
#include "assert.h"

static void *return_six(void *unused)
{
    (void)unused;
    return (void *)6;
}

// Basic validation of argument handling in pthread_join.
int test_join_argument_validation(void)
{
    pthread_t worker = 0;
    int create_status = pthread_create(&worker, NULL, return_six, NULL);
    assert(create_status == 0 && "test_join_argument_validation: worker creation failed");
    assert(worker != 0 && "test_join_argument_validation: worker id invalid");

    void *dummy = NULL;

    int join_result = pthread_join(0, &dummy);
    assert(join_result != 0 && "test_join_argument_validation: NULL thread id should fail");
    assert(dummy == NULL && "test_join_argument_validation: dummy should remain untouched on failure");

    join_result = pthread_join((pthread_t)27, &dummy);
    assert(join_result != 0 && "test_join_argument_validation: bogus thread id should fail");
    assert(dummy == NULL && "test_join_argument_validation: dummy should remain untouched on failure");

    join_result = pthread_join(worker, (void *)0x0000800000000000ULL);
    assert(join_result != 0 && "test_join_argument_validation: userspace pointer must be checked");

    join_result = pthread_join(worker, &dummy);
    assert(join_result == 0 && "test_join_argument_validation: valid join should succeed");
    assert((size_t)dummy == 6 && "test_join_argument_validation: worker return value mismatch");

    dummy = (void *)0xbeefbeef;
    join_result = pthread_join(worker, &dummy);
    assert(join_result != 0 && "test_join_argument_validation: joining collected thread should fail");
    assert(dummy == (void *)0xbeefbeef && "test_join_argument_validation: dummy must stay unchanged after failure");

    return 0;
}
