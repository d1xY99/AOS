#pragma once

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PIPE_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

ssize_t pipe_blocking_read(int fd, char *buffer, size_t len);

int test_roundtrip_single_thread(void);
int test_threaded_roundtrip(void);
int test_pipe_duplication(void);
int test_pipe_close_propagates(void);
int test_pipe_reuse_after_new_writes(void);
int test_pipe_large_transfer(void);
int test_pipe_eof_and_epipe(void);
int test_pipe_writer_blocks_until_reader_consumes(void);
int test_pipe_reader_blocks_until_writer_produces(void);
