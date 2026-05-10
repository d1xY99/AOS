#include <stdio.h>
#include <string.h>
#include <unistd.h>

__thread int t = 123;

int main(int argc, char **argv) {
  printf("Before exec: t=%d addr=%p\n", t, &t);

  execv("usr/tls_simple.elf", NULL);
  return 0;
}
