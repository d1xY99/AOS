#include <stdio.h>
#include <unistd.h>
#include <wait.h>

int aslr_exec_runner()
{
    printf("=== EXEC ASLR TEST ===\n");

    for (int i = 0; i < 5; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // Replace this process image with child program
            char *argv[] = {"/usr/exec_helper.elf", NULL};
            execv("/usr/exec_helper.elf", argv);

            // If exec fails
            printf("execv failed\n");
            _exit(1);
        }

        waitpid(pid, NULL, 0);
        printf("------- NEXT EXEC RUN -------\n");
    }

    return 0;
}
