#include "deadlock_pipe_common.h"

struct writer_ctx
{
	int fd;
	size_t remaining;
	int finished;
};

static void *writer_thread(void *arg)
{
	struct writer_ctx *ctx = (struct writer_ctx *)arg;
	char chunk[128];
	memset(chunk, 0xAB, sizeof(chunk));

	while (ctx->remaining > 0)
	{
		size_t to_write = ctx->remaining < sizeof(chunk) ? ctx->remaining
		                                                 : sizeof(chunk);
		ssize_t written = write(ctx->fd, chunk, to_write);
		if (written < 0)
		{
			ctx->finished = -1;
			return NULL;
		}
		ctx->remaining -= (size_t)written;
	}

	ctx->finished = 1;
	return NULL;
}

int test_full_pipe_unblock(void)
{
	printf("\n=== TEST 2: pipe full → read must unblock writer ===\n");

	int fds[2];
	TEST_ASSERT(pipe(fds) == 0, "pipe failed");

	struct writer_ctx ctx = {
	    .fd = fds[1],
	    .remaining = PIPE_BUF + 1,
	    .finished = 0,
	};

	pthread_t writer;
	TEST_ASSERT(pthread_create(&writer, NULL, writer_thread, &ctx) == 0,
	            "pthread_create failed");

	sleep(1);

	printf("Reader: freeing space...\n");
	char small;
	TEST_ASSERT(read(fds[0], &small, 1) == 1, "reader failed to read");
	printf("Reader: read() done → writer SHOULD continue\n");

	pthread_join(writer, NULL);
	TEST_ASSERT(ctx.finished == 1, "writer did not finish writing");

	close(fds[0]);
	close(fds[1]);
	return TEST_SUCCESS;
}
