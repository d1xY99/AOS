#include <unistd.h>
#include <stdio.h>

int main()
{
    int fds[2];
    pipe(fds);

    close(fds[1]); // writer closed BEFORE fork

    if (fork() == 0)
    {
        char x;
        // CHILD EXPECTED: read returns 0 immediately (EOF)
        int r = read(fds[0], &x, 1);
        printf("child read = %d\n", r);
        return 0;
    }
}
