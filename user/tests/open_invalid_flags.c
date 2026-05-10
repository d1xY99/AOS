#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    int fd;

    printf("Test start\n");
    fd = open("somefile.txt", 0xdeadbeef);
    printf("open returned: %d -> %s\n", fd, (fd == -1 ? "OK" : "FAIL"));

    return 0;
}
