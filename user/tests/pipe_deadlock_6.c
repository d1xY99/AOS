#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

int fds[2];

void *child(void *_)
{
    // Child still holds the writer FD
    // But calls exec → main thread will block forever
    execv("/usr/helloworld.elf", NULL);
    return NULL;
}

int main()
{
    pipe(fds);

    pthread_t th;
    pthread_create(&th, NULL, child, NULL);

    char buf[1];
    // EXPECTED: read should block forever because writer is still open
    // REAL TESTSYSTEM: must NOT deadlock because exec kills all threads
    ssize_t r = read(fds[0], buf, 1);
    printf("read returned = %ld\n", r);
}
