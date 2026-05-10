#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "string.h"

extern int alloc_pattern_test();
extern int alloc_sizes_test();
extern int alloc_realloc_test();
extern int alloc_randomized_test();
extern int alloc_concurrent_test();
extern int alloc_limits_test();

#define ALLOC_PATTERN 1 // test allocation patterns
#define ALLOC_SIZES 1   // test various allocation sizes
#define ALLOC_REALLOC 1 // test realloc functionality
#define ALLOC_RANDOM 1  // test randomized allocations
#define ALLOC_THREADS 1 // test concurrent allocations
#define ALLOC_LIMITS 1  // test allocation limits

int test_count = 0;
int passed_count = 0;

void print_test_header(const char *test_name, const char *description)
{
    printf("\n======================================\n");
    printf("TEST [%d] - %s\n", ++test_count, test_name);
    printf("Description: %s\n", description);
    printf("======================================\n");
}

void print_success(const char *test_name)
{
    printf(">>> %s ==== PASSED ====\n", test_name);
    passed_count++;
}

void print_failure(const char *test_name)
{
    printf(">>> %s ==== FAILED ====\n", test_name);
}

void print_summary()
{
    printf("\n========================================\n");
    printf("========== TEST SUMMARY ==========\n");
    printf("========================================\n");
    printf("Total Tests:    %d\n", test_count);
    printf("Tests Passed:   %d\n", passed_count);
    printf("Tests Failed:   %d\n", test_count - passed_count);
    printf("========================================\n");
}

int main()
{
    printf("\n============================================================\n");
    printf("===== MALLOC ALLOCATION TEST SUITE v1.0 =====\n");
    printf("============================================================\n\n");

    int retval = 0;

    if (ALLOC_PATTERN)
    {
        print_test_header("alloc_pattern_test", "Tests interleaved allocation patterns");
        retval = alloc_pattern_test();
        if (retval == 0)
        {
            print_success("alloc_pattern_test");
        }
        else
        {
            print_failure("alloc_pattern_test");
            return -1;
        }
    }

    if (ALLOC_SIZES)
    {
        print_test_header("alloc_sizes_test", "Tests various allocation sizes");
        retval = alloc_sizes_test();
        if (retval == 0)
        {
            print_success("alloc_sizes_test");
        }
        else
        {
            print_failure("alloc_sizes_test");
            return -1;
        }
    }

    if (ALLOC_REALLOC)
    {
        print_test_header("alloc_realloc_test", "Tests realloc functionality");
        retval = alloc_realloc_test();
        if (retval == 0)
        {
            print_success("alloc_realloc_test");
        }
        else
        {
            print_failure("alloc_realloc_test");
            return -1;
        }
    }

    if (ALLOC_RANDOM)
    {
        print_test_header("alloc_randomized_test", "Tests randomized allocations");
        retval = alloc_randomized_test();
        if (retval == 0)
        {
            print_success("alloc_randomized_test");
        }
        else
        {
            print_failure("alloc_randomized_test");
            return -1;
        }
    }

    if (ALLOC_THREADS)
    {
        print_test_header("alloc_concurrent_test", "Tests concurrent allocations");
        retval = alloc_concurrent_test();
        if (retval == 0)
        {
            print_success("alloc_concurrent_test");
        }
        else
        {
            print_failure("alloc_concurrent_test");
            return -1;
        }
    }

    if (ALLOC_LIMITS)
    {
        print_test_header("alloc_limits_test", "Tests allocation boundary conditions");
        retval = alloc_limits_test();
        if (retval == 0)
        {
            print_success("alloc_limits_test");
        }
        else
        {
            print_failure("alloc_limits_test");
            return -1;
        }
    }

    print_summary();

    if (passed_count == test_count)
    {
        printf("\n========================================\n");
        printf("===== ALL TESTS PASSED SUCCESSFULLY =====\n");
        printf("========================================\n\n");
        return 0;
    }
    else
    {
        printf("\n========================================\n");
        printf("===== SOME TESTS FAILED =====\n");
        printf("========================================\n\n");
        return -1;
    }
}
