#include <unistd.h>
#include <stdio.h>

int main()
{
    int fds[2];
    pipe(fds);

    if (fork() == 0)
    {
        if (fork() == 0)
        {
            return 0;
        }

        close(fds[1]);
        char x;
        int r = read(fds[0], &x, 1);
        printf("child read = %d\n", r);
        return 0;
    }

    sleep(1);
    close(fds[1]); // only writer
}
