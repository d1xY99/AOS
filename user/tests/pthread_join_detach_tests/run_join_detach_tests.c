#include "stdio.h"
#include "assert.h"
#include <unistd.h>
#include "pthread.h"

typedef int (*join_test_fn)(void);

typedef struct
{
    const char *label;
    const char *description;
    join_test_fn fn;
} join_test_case;

extern int test_join_completed_worker_returns_value(void);
extern int test_join_waits_with_value_ptr(void);
extern int test_join_waits_without_value_ptr(void);
extern int test_join_loop_multiple_threads(void);
extern int test_join_second_attempt_fails(void);
extern int test_join_collects_multiple_sums(void);
extern int test_join_cancel_interaction(void);
extern int test_join_void_worker(void);
extern int test_join_argument_validation(void);
extern int test_join_self_and_rejoin(void);
extern int test_join_returns_null_pointer(void);
extern int test_detach_error_paths(void);
extern int test_detach_with_waiting_thread(void);
extern int test_detach_not_joinable(void);
extern int test_detach_self_twice(void);
extern int test_detach_self_twice(void);

static void *helper_detached_worker(void *unused)
{
    (void)unused;
    sleep(1);
    return (void *)0x44;
}

static int test_detach_join_after_exit(void)
{
    size_t worker = 0;
    assert(pthread_create(&worker, NULL, helper_detached_worker, NULL) == 0 &&
           "test_detach_join_after_exit: worker creation failed");
    assert(worker != 0 && "test_detach_join_after_exit: worker id invalid");

    assert(pthread_detach(worker) == 0 &&
           "test_detach_join_after_exit: detach should succeed");

    sleep(2);

    void *sentinel = (void *)0x123;
    int join_status = pthread_join(worker, &sentinel);
    assert(join_status != 0 && "test_detach_join_after_exit: detached worker must not be joinable");
    assert(sentinel == (void *)0x123 && "test_detach_join_after_exit: sentinel should remain unchanged");

    return 0;
}

extern void test_join_invalid_value_ptr_should_abort(void);

static const join_test_case k_join_detach_tests[] = {
    {"completed_worker", "collect the value from a finished worker", test_join_completed_worker_returns_value},
    {"waits_with_value", "block until worker completes (value_ptr)", test_join_waits_with_value_ptr},
    {"waits_without_value", "block until worker completes (NULL value_ptr)", test_join_waits_without_value_ptr},
    // {"loop_multiple_threads", "start many workers sequentially", test_join_loop_multiple_threads},
    {"second_attempt_fails", "joining the same thread twice should fail", test_join_second_attempt_fails},
    {"collects_multiple_sums", "validate sum return values across workers", test_join_collects_multiple_sums},
    {"cancel_interaction", "cancel a joiner and clean up correctly", test_join_cancel_interaction},
    {"void_worker", "join workers that return no value", test_join_void_worker},
    {"argument_validation", "exercise invalid pthread_join parameters", test_join_argument_validation},
    {"self_and_rejoin", "joining self and rejoining a worker", test_join_self_and_rejoin},
    {"returns_null_pointer", "ensure NULL result propagates", test_join_returns_null_pointer},
    {"detach_error_paths", "basic detach error handling", test_detach_error_paths},
    {"detach_with_waiter", "detach thread while another join waits", test_detach_with_waiting_thread},
    {"detach_not_joinable", "detached threads should not be joinable", test_detach_not_joinable},
    {"detach_join_after_exit", "joining after detached worker exits", test_detach_join_after_exit},
    {"detach_self_twice", "detaching self twice and detached worker join", test_detach_self_twice},
};

static void print_result_banner(const char *label, const char *description, int index, int status)
{
    printf("-------------------------------------------------------------\n");
    printf("[%02d] %s: %s\n", index, label, status == 0 ? "ok" : "FAILED");
    printf("Detail: %s\n", description);
    printf("_____________________________________________________________\n\n");
}

static int execute_case(const join_test_case *tc, int index)
{
    int rc = tc->fn();
    print_result_banner(tc->label, tc->description, index, rc);
    return rc == 0;
}

int main(void)
{
    const size_t test_count = sizeof(k_join_detach_tests) / sizeof(k_join_detach_tests[0]);
    size_t passed = 0;

    for (size_t i = 0; i < test_count; ++i)
    {
        passed += execute_case(&k_join_detach_tests[i], (int)i + 1);
    }

    if (passed == test_count)
    {
        printf("\nAll pthread join/detach tests passed (%zu/%zu).\n", passed, test_count);
    }
    else
    {
        printf("\nJoin/detach tests failed (%zu/%zu).\n", passed, test_count);
        assert(0 && "run_join_detach_tests: at least one testcase failed");
    }

    test_join_invalid_value_ptr_should_abort();
    assert(0 && "run_join_detach_tests: process should have been terminated by invalid join");
}
