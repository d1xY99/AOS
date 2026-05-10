#include <stdio.h>
#include <semaphore.h>

int main() {
    sem_t sem;

    printf("Test start\n");
    int init = sem_init(&sem, 0, 1);
    printf("sem_init returned: %d -> %s\n", init, (init == 0 ? "OK" : "FAIL"));

    int t1 = sem_trywait(&sem);
    printf("sem_trywait #1 returned: %d -> %s\n", t1, (t1 == 0 ? "OK" : "FAIL"));

    int t2 = sem_trywait(&sem);
    printf("sem_trywait #2 returned: %d -> %s\n", t2, (t2 == -1 ? "OK" : "FAIL"));

    int d = sem_destroy(&sem);
    printf("sem_destroy returned: %d -> %s\n", d, (d == 0 ? "OK" : "FAIL"));

    return 0;
}
