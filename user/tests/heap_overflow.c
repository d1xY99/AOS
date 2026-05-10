#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define max_forks 40
/*
 * This stress test repeatedly forks without reaping children in order to
 * exhaust the kernel heap. Each child blocks in an infinite sleep so that
 * its kernel-side structures remain allocated. Once the heap is depleted the
 * kernel will hit:
 *
 *   assert((reserved_max_ == 0 || ((kernel_break_ - base_break_) + size)
 *          <= reserved_max_) && "maximum kernel heap size reached");
 *
 * Expect the kernel to panic when running this test.
 */
int main(void)
{
    size_t spawned = 0;

    for (size_t i = 0; i < max_forks; i++)
    {
        size_t pid = fork();
        if (pid < 0)
        {
            printf("fork failed after %zu children\n", spawned);
            return -1;
        }
    }
}
