// close_invalid.c
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    int ret;
    const char *fname = "usr/local_fd_test.txt";

    printf("Test: close(-1)\n");
    ret = close(-1);
    printf("    close(-1) returned: %d -> %s\n", ret, (ret == -1 ? "OK" : "FAIL"));

    printf("Test: close on never opened fd\n");
    ret = close(1);
    printf("    close(1) returned: %d -> %s\n", ret, (ret == -1 ? "OK" : "FAIL"));

    printf("Test: double close on a valid fd\n");
    int fd = open(fname, O_CREAT | O_RDWR);
    printf("    open returned: %d -> %s\n", fd, (fd >= 0 ? "OK" : "FAIL"));

    if (fd >= 0) {
        int r1 = close(fd);
        int r2 = close(fd);
        printf("    first close returned: %d -> %s\n", r1, (r1 == 0 ? "OK" : "FAIL"));
        printf("    second close returned: %d -> %s\n", r2, (r2 == -1 ? "OK" : "FAIL"));
    }

    return 0;
}
