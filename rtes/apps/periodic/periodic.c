#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <stdint.h>
#include <sys/syscall.h>

const uint64_t CLOCKS_PER_MSEC = (uint64_t)CLOCKS_PER_SEC / 1000;
const uint64_t CLOCKS_PER_NSEC = (uint64_t)CLOCKS_PER_SEC / 1000000000;

// Main function: entry point for execution
int main(int argc, char *argv[]) {

    uint64_t next_wakeup = clock();
    
    //check if there is the same amount of arguments
    if (argc != 4) {
        printf("Usage: %s <C (ms)> <T (ms)> <cpuid>\n", argv[0]);
        return -1;
    }

    int cArg = atoi(argv[1]);
    int tArg = atoi(argv[2]);
    int cpuArg = atoi(argv[3]);
    struct timespec sleep_time;
    uint64_t clocks_between_periods = tArg * CLOCKS_PER_MSEC;

    // Setting up CPU affinity using syscall for sched_setaffinity
    unsigned long cpumask = 1UL << cpuArg;  // Set affinity to specified CPU

    if (syscall(__NR_sched_setaffinity, 0, sizeof(cpumask), &cpumask) == -1) {
        perror("sched_setaffinity syscall");
        return -1;
    }


    while(1)
    {
        next_wakeup += clocks_between_periods;
        uint64_t start = clock();
        uint64_t totalElpasedTime = 0;

        //pretend to do busy work
        while(cArg >= totalElpasedTime)
        {
            totalElpasedTime = (clock() -  start ) / CLOCKS_PER_MSEC;
        }

        //set sleep time struct to go to sleep
        uint64_t sleep_clock_cycles = next_wakeup - clock();

        sleep_time.tv_sec =  sleep_clock_cycles / CLOCKS_PER_SEC;
        sleep_time.tv_nsec = (sleep_clock_cycles % CLOCKS_PER_SEC) / CLOCKS_PER_NSEC;

        nanosleep(&sleep_time, NULL);

    }

    return 0;
}
