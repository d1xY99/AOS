#pragma once

#include "unistd.h"

int reset_file_contents(const char *path, const char *payload);
int read_file_into_buffer(const char *path, char *buffer, int capacity);
int wait_for_child(pid_t child_pid, int *exit_status);
