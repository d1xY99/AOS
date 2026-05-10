#include <stdio.h>

int main(int argc, char *argv[]) {
  printf("Executing exec_print_arguments with arguments!\n");

  printf("Number of arguments: %d\n", argc);

  if (argc == 0) {
    printf("No arguments provided.\n");
    return 0;
  }

  printf("Address of argv array: %p\n", (void *)argv);
  printf("Address of argv[0]: %p\n", (void *)argv[0]);

  printf("Printing arguments:\n");
  for (int i = 0; i < argc; i++) {
    printf("Argument %d: %s\n", i, argv[i]);
  }

  return 0;
}
