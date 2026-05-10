#include <pthread.h>
#include <stdio.h>

__thread size_t tls_variable = 420;
__thread size_t tls_var2 = 1;
__thread size_t tls_var3;
__thread int tls_var4;

int main() {

  printf("Trying to read TLS variable...\n");

  printf("TLS variable value: %zu\n", tls_variable);
  printf("TLS variable 2 value: %zu\n", tls_var2);
  printf("TLS variable 3 value (uninitialized): %zu\n", tls_var3);
  printf("TLS variable 4 value (uninitialized): %d\n", tls_var4);

  // print addresses of the TLS variables
  printf("Address of TLS variable: %p\n", (void *)&tls_variable);
  printf("Address of TLS variable 2: %p\n", (void *)&tls_var2);
  printf("Address of TLS variable 3: %p\n", (void *)&tls_var3);
  printf("Address of TLS variable 4: %p\n", (void *)&tls_var4);

  // check if adderss is below userbreak limit

  if ((size_t)&tls_variable < USER_BREAK && (size_t)&tls_var2 < USER_BREAK &&
      (size_t)&tls_var3 < USER_BREAK && (size_t)&tls_var4 < USER_BREAK) {
    printf("All TLS variable addresses are below userbreak limit.\n");
  } else {
    printf(
        "Error: One or more TLS variable addresses exceed userbreak limit!\n");
  }

  return 0;
}
