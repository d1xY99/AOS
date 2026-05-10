// access_test.c
#include "stdio.h"
#include "unistd.h"
#include "fcntl.h"
#include <string.h>

static void print_access_result(const char *path, int mode, const char *mode_name)
{
    int ret = access(path, mode);
    printf("access(\"%s\", %s) = %d\n", path, mode_name, ret);
}

int main(void)
{
	const char *missing = "does_not_exist.txt";

  printf("Start test\n");

	printf("Non existing file:\n");
	print_access_result(missing, F_OK, "F_OK");
	print_access_result(missing, R_OK, "R_OK");
	print_access_result(missing, W_OK, "W_OK");
	print_access_result(missing, X_OK, "X_OK");

	printf("\n");

  const char *file_write = "/usr/test_write.txt";
  const char *file_read = "/usr/test_read.txt";

  int fd1 = open(file_write, O_WRONLY | O_CREAT);
  int fd2 = open(file_read, O_RDONLY | O_CREAT);

  if(fd1 < 0 || fd2 < 0)
  {
    printf("FAIL: Open of files\n");
    return 0;
  }
  else
    printf("OK: source file opened/created\n");

	printf("\nExisting file:\n");

	printf("\nWrite file:\n");
	print_access_result(file_write, F_OK, "F_OK");
	print_access_result(file_write, R_OK, "R_OK");
	print_access_result(file_write, W_OK, "W_OK");
	print_access_result(file_write, X_OK, "X_OK");


	printf("\nRead file:\n");
	print_access_result(file_read, F_OK, "F_OK");
	print_access_result(file_read, R_OK, "R_OK");
	print_access_result(file_read, W_OK, "W_OK");
	print_access_result(file_read, X_OK, "X_OK");

  printf("test PASSED\n");

	return 0;
}
