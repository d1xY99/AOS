#include "deadlock_pipe_common.h"

struct reader_ctx
{
	int fd;
	int result;
};

static void *reader_thread(void *arg)
{
	struct reader_ctx *ctx = (struct reader_ctx *)arg;
	char b;
	ssize_t r = read(ctx->fd, &b, 1);
	printf("Reader returned: %d (expected 0)\n", (int)r);
	ctx->result = (int)r;
	return NULL;
}

int test_close_writer_wakes_reader(void)
{
	printf("\n=== TEST 3: close writer → reader MUST wake ===\n");

	int fds[2];
	TEST_ASSERT(pipe(fds) == 0, "pipe failed");

	struct reader_ctx ctx = {.fd = fds[0], .result = -1};
	pthread_t reader;
	TEST_ASSERT(pthread_create(&reader, NULL, reader_thread, &ctx) == 0,
	            "pthread_create failed");

	sleep(1);
	printf("Parent closing writer...\n");
	close(fds[1]);

	pthread_join(reader, NULL);
	TEST_ASSERT(ctx.result == 0, "reader did not see EOF");

	close(fds[0]);
	return TEST_SUCCESS;
}
