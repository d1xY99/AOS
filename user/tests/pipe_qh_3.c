#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main()
{
    int fd[2];
    pipe(fd);

    int pid = fork();

    if (pid == 0)
    {
        // child
        int w = write(fd[1], "AAA", 3);
        printf("child write returned = %d\n", w);
        exit(0);
    }
    else
    {
        // only the parent drops its read end; the child still keeps its copy
        close(fd[0]);
        waitpid(pid, NULL, 0);
        printf("parent done\n");
    }
}
