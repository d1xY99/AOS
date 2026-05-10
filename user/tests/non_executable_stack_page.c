
#include <stdio.h>

int main(void) {
  char c = 0xC3; // 0xC3 = RET -> Return from procedure

  void *target = &c;

  printf("Stack address: %p\n", target);
  printf("Attempting to execute a single char on the stack...\n");

  __asm__ volatile("call *%0" : : "r"(target) : "rax");

  puts("If you see this, your stack is executable.");
  return 0;
}
