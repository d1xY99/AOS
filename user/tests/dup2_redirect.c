#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void exercise_replacing_existing_descriptor(void) {
  int first[2];
  int second[2];
  assert(pipe(first) == 0);
  assert(pipe(second) == 0);

  const char payload[] = "payload-through-dup2";
  assert(write(first[1], payload, sizeof(payload)) == (ssize_t)sizeof(payload));

  assert(dup2(first[0], second[0]) == second[0]);

  char buffer[sizeof(payload)] = {0};
  assert(read(second[0], buffer, sizeof(buffer)) == (ssize_t)sizeof(payload));
  assert(memcmp(buffer, payload, sizeof(payload)) == 0);

  assert(close(first[0]) == 0);
  assert(close(first[1]) == 0);
  assert(close(second[0]) == 0);
  assert(close(second[1]) == 0);
}

static void exercise_chained_dup2(void) {
  int source[2];
  int sink[2];
  assert(pipe(source) == 0);
  assert(pipe(sink) == 0);

  const char line[] = "dup2 chain works";

  assert(dup2(source[1], sink[1]) == sink[1]);
  assert(close(source[1]) == 0);

  assert(write(sink[1], line, sizeof(line)) == (ssize_t)sizeof(line));
  assert(close(sink[1]) == 0);

  char buffer[sizeof(line)] = {0};
  assert(read(source[0], buffer, sizeof(buffer)) == (ssize_t)sizeof(line));
  assert(memcmp(buffer, line, sizeof(line)) == 0);
  assert(close(source[0]) == 0);
  assert(close(sink[0]) == 0);
}

int main(void) {
  exercise_replacing_existing_descriptor();
  exercise_chained_dup2();
  assert(dup2(-1, 5) == -1);
  printf("dup2_redirect passed\n");
  return 0;
}
