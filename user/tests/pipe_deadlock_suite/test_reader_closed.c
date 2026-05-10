#include "deadlock_pipe_common.h"

int test_reader_closed(void)
{
	printf("\n=== TEST 5: reader closed → writer MUST get EPIPE ===\n");

	int fds[2];
	TEST_ASSERT(pipe(fds) == 0, "pipe failed");

	pid_t pid = fork();
	TEST_ASSERT(pid >= 0, "fork failed");
	if (pid == 0)
	{
		close(fds[0]);
		_exit(0);
	}

	sleep(1);

	char b = 'X';
	ssize_t r = write(fds[1], &b, 1);
	printf("write returned: %d (expected -1)\n", (int)r);
	TEST_ASSERT(r == -1, "write did not fail");

	waitpid(pid, NULL, 0);
	close(fds[0]);
	close(fds[1]);
	return TEST_SUCCESS;
}
