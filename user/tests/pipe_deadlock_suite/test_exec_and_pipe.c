#include "deadlock_pipe_common.h"

static void *exec_thread(void *arg)
{
	(void)arg;
	execv("/usr/helloworld.elf", NULL);
	return NULL;
}

static int run_exec_and_pipe_case(void)
{
	printf("\n=== TEST 1: exec + pipe + threads === (child)\n");

	int fds[2];
	TEST_ASSERT(pipe(fds) == 0, "pipe failed");

	pthread_t th;
	TEST_ASSERT(pthread_create(&th, NULL, exec_thread, &fds[1]) == 0,
				"pthread_create failed");

	char buf[8];
	printf("Main calling read()...\n");
	ssize_t n = read(fds[0], buf, sizeof(buf));
	printf("read returned: %d\n", (int)n);
	TEST_ASSERT(n >= 0, "read returned error");

	pthread_join(th, NULL);
	close(fds[0]);
	close(fds[1]);
	return TEST_SUCCESS;
}

int test_exec_and_pipe(void)
{
	pid_t pid = fork();
	TEST_ASSERT(pid >= 0, "fork failed");
	if (pid == 0)
	{
		int rc = run_exec_and_pipe_case();
		_exit(rc);
	}

	int status = 0;
	TEST_ASSERT(waitpid(pid, &status, 0) == pid, "waitpid failed");
	return status;
}
