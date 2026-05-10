#include <stdio.h>

__thread int tls_var;

int tlsRead(void) {

  printf("Trying to read TLS variable...\n");

  size_t tls_address = (size_t)&tls_var;

  printf("TLS variable address: %zx\n", tls_address);

  return 0;
}
