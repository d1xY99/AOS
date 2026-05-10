#include <unistd.h>
#include <stdio.h>

int main()
{
    int fds[2];
    pipe(fds);

    if (fork() == 0)
    {
        // child writer
        sleep(2);
        return 0;
    }

    // parent writes
    sleep(1);
    close(fds[1]); // parent closes

    char x;
    int r = read(fds[0], &x, 1);
    printf("read = %d (should block until child exits)\n", r);
}
