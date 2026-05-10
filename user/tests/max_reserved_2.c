#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#define DEPTH_LIMIT 280

// This function creates one child thread and waits for it
void *create_next(void *arg)
{
    int depth = *(int *)arg;

    if (depth >= DEPTH_LIMIT)
    {
        printf("Reached max depth: %d\n", depth);
        return NULL;
    }

    pthread_t child;
    int next_depth = depth + 1;

    // Try to create next thread
    int rv = pthread_create(&child, NULL, create_next, &next_depth);
    if (rv != 0)
    {
        printf("Failed to create thread at depth %d (error %d)\n", depth, rv);
        return NULL;
    }

    // Wait for child to finish
    pthread_join(child, NULL);

    // Just return
    return NULL;
}

int main()
{
    pthread_t root;
    int start_depth = 1;

    printf("Starting nested pthread test up to %d levels...\n", DEPTH_LIMIT);

    int rv = pthread_create(&root, NULL, create_next, &start_depth);
    assert(rv == 0);

    pthread_join(root, NULL);

    printf("Test finished.\n");
    return 0;
}
