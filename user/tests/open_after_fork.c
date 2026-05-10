#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <wait.h>
#include <stdio.h>

int main(void)
{
    const char *fname = "usr/fd_inherit_test.txt";
    int fd = open(fname, O_WRONLY | O_CREAT);
    assert(fd >= 0);

    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0)
    {
        // child: should not see the fd opened after fork
        sleep(1);
        int test_fd = 8; // same slot as parent’s next fd
        char buf[4];
        int r = read(test_fd, buf, sizeof(buf));
        assert(r == -1); // should fail (EBADF)
        printf("[PASS] Child cannot read parent’s fd opened after fork\n");
        _exit(0);
    }
    else
    {
        int status = 0;
        // parent: open new file AFTER fork
        int newfd = open("usr/after_fork.txt", O_RDONLY | O_CREAT);
        printf("new fd : %d\n", newfd);
        assert(newfd >= 0);
        waitpid(pid, &status, 0);
    }
    return 0;
}
