#include "deadlock_pipe_common.h"

struct test_case
{
	const char *name;
	int (*func)(void);
};

int main(void)
{
	const struct test_case tests[] = {
		// {"exec + pipe + threads", test_exec_and_pipe},
		{"pipe full unblocks writer", test_full_pipe_unblock},
		{"closing writer wakes reader", test_close_writer_wakes_reader},
		{"fork inherits only calling thread", test_fork_writer_inheritance},
		{"reader closed -> writer sees EPIPE", test_reader_closed},
	};

	size_t failures = 0;
	for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
	{
		printf("[deadlock-pipe] RUN %s\n", tests[i].name);
		int rc = tests[i].func();
		if (rc == TEST_SUCCESS)
		{
			printf("[deadlock-pipe] OK  %s\n", tests[i].name);
		}
		else
		{
			++failures;
			printf("[deadlock-pipe] ERR %s\n", tests[i].name);
		}
	}

	if (failures)
	{
		printf("[deadlock-pipe] %zu/%zu tests FAILED\n", failures,
			   sizeof(tests) / sizeof(tests[0]));
		return 1;
	}

	printf("[deadlock-pipe] all %zu tests passed\n",
		   sizeof(tests) / sizeof(tests[0]));
	return 0;
}
