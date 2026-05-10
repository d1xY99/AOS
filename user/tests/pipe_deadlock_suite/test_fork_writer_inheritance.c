#include "deadlock_pipe_common.h"

struct sleeper_ctx
{
	volatile int running;
};

static void *sleeper_thread(void *arg)
{
	struct sleeper_ctx *ctx = (struct sleeper_ctx *)arg;
	while (ctx->running)
	{
		sleep(1);
	}
	return NULL;
}

int test_fork_writer_inheritance(void)
{
	printf("\n=== TEST 4: fork should NOT inherit other threads ===\n");

	int fds[2];
	TEST_ASSERT(pipe(fds) == 0, "pipe failed");

	struct sleeper_ctx ctx = {.running = 1};
	pthread_t th;
	TEST_ASSERT(pthread_create(&th, NULL, sleeper_thread, &ctx) == 0,
	            "pthread_create failed");

	pid_t pid = fork();
	TEST_ASSERT(pid >= 0, "fork failed");
	if (pid == 0)
	{
		close(fds[1]);
		printf("Child reading...\n");
		char x;
		ssize_t r = read(fds[0], &x, 1);
		printf("Child read returned: %d (should be 0)\n", (int)r);
		_exit(0);
	}

	sleep(1);
	close(fds[1]);
	waitpid(pid, NULL, 0);

	ctx.running = 0;
	pthread_join(th, NULL);
	close(fds[0]);
	return TEST_SUCCESS;
}
