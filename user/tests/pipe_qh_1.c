#include <stdio.h>
#include <unistd.h>
#include <assert.h>

int main()
{
    int fd[2];
    assert(pipe(fd) == 0);

    printf("Before fork: fd[0]=%d, fd[1]=%d\n", fd[0], fd[1]);

    int pid = fork();
    assert(pid >= 0);

    if (pid == 0)
    {
        // CHILD
        printf("Child sees: fd[0]=%d, fd[1]=%d\n", fd[0], fd[1]);

        char m[] = "X";
        int w = write(fd[1], m, 1);
        printf("Child write: %d\n", w);

        exit(0);
    }
    else
    {
        // PARENT
        char b[4] = {0};
        int r = read(fd[0], b, 1);
        printf("Parent read: %d (%c)\n", r, b[0]);
    }
}
