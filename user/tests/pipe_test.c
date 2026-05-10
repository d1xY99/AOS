#include "unistd.h"
#include "stdio.h"
#include "string.h"

int main() {
    int fds[2];
    printf("call pipe:\n");
    int c = pipe(fds);
    printf("called pipe:\n");
    if (c < 0) {
        printf("pipe failed\n");
        return 1;
    }
    printf("finished creating pipe\n");

    printf("write\n");
    write(fds[1], "Its writen\n", 8);

    printf("call fork\n");
    size_t pid = fork();
    if (pid == 0) {
        //child reads
        char buf[64];
        printf("call read\n");
        int n = read(fds[0], buf, 69);
        buf[n] = '\0';
        printf("child got from pipe: %s\n", buf);
        printf("finished\n");
        _exit(0);
    } else {
        //parent writes
        printf("write inside the pipe: something\n");
        write(fds[1], "something", 8);
        //waitpid(pid, 0, 0);
    }
    return 0;
}
