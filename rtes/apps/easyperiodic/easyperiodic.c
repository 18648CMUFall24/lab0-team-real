#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <signal.h>
#include "easyperiodic.h"
#include "../reserve/reserve.h"

const uint64_t CLOCKS_PER_MSEC = (uint64_t)CLOCKS_PER_SEC / 1000;
const uint64_t CLOCKS_PER_NSEC = (uint64_t)CLOCKS_PER_SEC / 1000000000;

void handle_sigexcess() {
    printf("SIGEXCESS recieved\n");
    exit(-1);
}

// Main function: entry point for execution
int main(int argc, char *argv[]) {
    struct timespec cost, period;

    //check if there is the same amount of arguments
    if (argc != 4) {
        printf("Usage: %s <C (ms)> <T (ms)> <cpuid>\n", argv[0]);
        return -1;
    }

    int cArg = atoi(argv[1]);
    int tArg = atoi(argv[2]);
    int cpuArg = atoi(argv[3]);


    if (tArg < cArg) {
        perror("Period less than Execution Budget");
        return -1;
    }

    uint64_t clocks_running = cArg * CLOCKS_PER_MSEC;

    time_t cost_s = cArg / 1000;
    long int cost_ns = (cArg % 1000) * 1000000000;

    time_t period_s = tArg / 1000;
    long int period_ns = (tArg % 1000) * 1000000000;

    signal(SIGEXCESS, handle_sigexcess);

    cost.tv_sec = cost_s;
    cost.tv_nsec = cost_ns + 5000000; // Give an extra half ms of buffer
    period.tv_sec = period_s;
    period.tv_nsec = period_ns;

    printf("Cost: %d s %ld ns\n", cost_s, cost_ns);
    printf("Period: %d s %ld ns\n", period_s, period_ns);

    set_reserve(0, &cost, &period, cpuArg);

    while(1)
    {
        clock_t start = clock();
        clock_t clocks_periodElapsedTime = 0;

        printf("Started period...");
        fflush(stdout);

        //pretend to do busy work
        while(clocks_periodElapsedTime < clocks_running)
        {
            clocks_periodElapsedTime = (clock() - start);
        }

        printf("finished work.\n");
        end_job();
    }

    return 0;
}
