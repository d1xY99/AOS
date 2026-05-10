#pragma once

// #include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <unistd.h>

#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif

#define TEST_SUCCESS 0
#define TEST_FAILURE 1

#define TEST_ASSERT(cond, msg)          \
	do                                  \
	{                                   \
		if (!(cond))                    \
		{                               \
			printf("ERROR: %s\n", msg); \
			return TEST_FAILURE;        \
		}                               \
	} while (0)

int test_exec_and_pipe(void);
int test_full_pipe_unblock(void);
int test_close_writer_wakes_reader(void);
int test_fork_writer_inheritance(void);
int test_reader_closed(void);
