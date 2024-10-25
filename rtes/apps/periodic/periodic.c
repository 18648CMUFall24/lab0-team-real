#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <stdint.h>
#include <sys/syscall.h>

// Main function: entry point for execution
int main(int argc, char *argv[]) {
    
    //check if there is the same amount of arguments
    if (argc != 4) {
        printf("Usage: %s <C (ms)> <T (ms)> <cpuid>\n", argv[0]);
        return -1;
    }

    int cArg = atoi(argv[1]);
    int tArg = atoi(argv[2]);
    int cpuArg = atoi(argv[3]);
    struct timespec sleep_time;
    uint64_t clocks_per_msec = (double)CLOCKS_PER_SEC / 1000;

    // Setting up CPU affinity using syscall for sched_setaffinity
    unsigned long cpumask = 1UL << cpuArg;  // Set affinity to specified CPU

    if (syscall(__NR_sched_setaffinity, 0, sizeof(cpumask), &cpumask) == -1) {
        perror("sched_setaffinity syscall");
        return -1;
    }


    while(1)
    {
        uint64_t start = clock();
        uint64_t totalElpasedTime = (clock() -  start ) / clocks_per_msec;

        //pretend to do busy work
        while(cArg >= totalElpasedTime)
        {
            totalElpasedTime = (clock() -  start ) / clocks_per_msec;
        }

        //set sleep time struct to go to sleep
        sleep_time.tv_sec = (tArg- cArg) / 1000;
        sleep_time.tv_nsec = ((tArg- cArg) % 1000)* 1000000;

        nanosleep(&sleep_time, NULL);

    }

    return 0;
}
