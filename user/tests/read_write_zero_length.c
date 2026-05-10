#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    printf("Test start\n");
    int fd = open("/rw.txt", O_CREAT | O_RDWR);
    printf("open returned: %d -> %s\n", fd, (fd >= 0 ? "OK" : "FAIL"));

    char buf[16];

    for (int i = 0; i < 16; i++)
        buf[i] = 'X';

    printf("write(fd, buf, 0)\n");
    int w = write(fd, buf, 0);
    printf("write returned: %d -> %s\n", w, (w == 0 ? "OK" : "FAIL"));

    printf("read(fd, buf, 0)\n");
    int r = read(fd, buf, 0);
    printf("read returned: %d -> %s\n", r, (r == 0 ? "OK" : "FAIL"));

    //check buffer unchanged
    int unchanged = 1;
    for (int i = 0; i < 16; i++) {
        if (buf[i] != 'X')
            unchanged = 0;
    }

    printf("buffer unchanged -> %s\n", unchanged ? "OK" : "FAIL");
    close(fd);
    return 0;
}
