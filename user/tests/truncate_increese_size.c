#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

static long get_size_fd(int fd)
{
    long cur = lseek(fd, 0, SEEK_CUR);
    long end = lseek(fd, 0, SEEK_END);
    lseek(fd, cur, SEEK_SET);
    return end;
}

int main()
{
    const char* path = "/usr/truncate_test.txt";

    int fd = open(path, O_CREAT | O_RDWR);
    if (fd < 0) {
        printf("FAIL: open returned %d\n", fd);
        return 1;
    }

    int w = write(fd, "Hello", 5);
    printf("write returned %d (expected 5) -> %s\n", w, (w == 5) ? "OK" : "FAIL");

    int r1 = ftruncate(fd, 12);

    printf("ftruncate(fd,12) returned %d -> %s\n", r1, (r1 == 0) ? "OK" : "FAIL");

    printf("size check expected 12 -> %s\n", (get_size_fd(fd) == 12) ? "OK" : "FAIL");

    close(fd);

    int r2 = truncate(path, 20);
    printf("truncate(path,20) returned %d -> %s\n", r2, (r2 == 0) ? "OK" : "FAIL");

    // reopen to verify size after truncate
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("FAIL: reopen returned %d\n", fd);
        return 1;
    }

    printf("final size check expected 20 -> %s\n", (get_size_fd(fd) == 20) ? "OK" : "FAIL");

    close(fd);
    return 0;
}
