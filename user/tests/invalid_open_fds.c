#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

int main(void)
{
    const char *fname = "usr/local_fd_test.txt";
    const char data[] = "LOCAL_FD_TEST";
    const int len = sizeof(data) - 1;

    // 1. Create file and write known data
    int fd_w = open(fname, O_WRONLY | O_CREAT);
    assert(fd_w >= 0);
    int w = write(fd_w, data, len);
    assert(w == len);
    int c = close(fd_w);
    assert(c == 0);

    // 2. Open read-only fd (this one should be "local" and not inherited)
    int fd_local = open(fname, O_RDONLY);
    assert(fd_local >= 0);

    // sanity: read a few bytes in parent
    char buf_p[8] = {0};
    int r = read(fd_local, buf_p, 5);
    assert(r == 5);
    assert(memcmp(buf_p, "LOCAL", 5) == 0);

    // rewind
    off_t pos = lseek(fd_local, 0, SEEK_SET);
    assert(pos == 0);

    // 3. Fork child
    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0)
    {
        // ----- CHILD -----
        char buf_c[16] = {0};
        int rc = read(fd_local, buf_c, sizeof(buf_c));

        if (rc >= 0)
        {
            // BUG: child could read parent’s local fd
            write(1, "[FAIL] Child unexpectedly read from parent's local fd\n", 54);
            write(1, "Data read: ", 11);
            write(1, buf_c, rc);
            write(1, "\n", 1);
            assert(0 && "Child should not read from parent's local fd");
        }

        // try to close — should also fail (fd invalid)
        int cl = close(fd_local);
        if (cl == 0)
        {
            write(1, "[FAIL] Child could close parent's local fd\n", 43);
            assert(0 && "Child closed parent's local fd");
        }

        _exit(0);
    }

    // ----- PARENT -----
    int status = 0;
    pid_t wpid = waitpid(pid, &status, 0);
    assert(wpid == pid);
    // assert(WIFEXITED(status));
    // assert(WEXITSTATUS(status) == 0);

    // ensure fd still works in parent
    char buf2[16] = {0};
    int r2 = read(fd_local, buf2, len);
    assert(r2 > 0);

    // cleanup
    int cl2 = close(fd_local);
    assert(cl2 == 0);
    unlink(fname);

    write(1, "[PASS] Local fd not inherited by child\n", 39);
    return 0;
}
// #include "test_support.h"

// int reset_file_contents(const char *path, const char *payload)
// {
//     int fd = open(path, O_WRONLY | O_CREAT);
//     if (fd < 0)
//     {
//         return -1;
//     }

//     size_t total = strlen(payload);
//     ssize_t written = write(fd, payload, total);
//     close(fd);
//     return (written == (ssize_t)total) ? 0 : -1;
// }

// int read_file_into_buffer(const char *path, char *buffer, int capacity)
// {
//     int fd = open(path, O_RDONLY);
//     if (fd < 0)
//     {
//         return -1;
//     }

//     ssize_t received = read(fd, buffer, capacity - 1);
//     close(fd);
//     if (received < 0)
//     {
//         return -1;
//     }

//     buffer[received] = '\0';
//     return 0;
// }

// int wait_for_child(pid_t child_pid, int *exit_status)
// {
//     int status = 0;
//     if (waitpid(child_pid, &status, 0) != child_pid)
//     {
//         return -1;
//     }

//     if (exit_status)
//     {
//         *exit_status = status;
//     }
//     return 0;
// }
