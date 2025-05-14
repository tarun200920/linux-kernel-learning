#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>

int main()
{
    struct sched_param param;
    param.sched_priority = 99;

    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler");
        return 1;
    }

    while (1)
        printf("Running real-time process...\n");

    return 0;
}
