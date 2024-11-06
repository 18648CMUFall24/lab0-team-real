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

const uint64_t CLOCKS_PER_MSEC = (uint64_t)CLOCKS_PER_SEC / 1000;
const uint64_t CLOCKS_PER_NSEC = (uint64_t)CLOCKS_PER_SEC / 1000000000;

void handle_sigexcess() {
    printf("SIGEXCESS recieved\n");
    exit(-1);
}

void increment_by_period(struct timeval *time, uint64_t period_s, int64_t period_us) {
    time->tv_sec += period_s;
    time->tv_usec += period_us;
    time->tv_sec += time->tv_usec / 1000000;
    time->tv_usec %= 1000000;
}

// Returns a negative value if bad
int set_sleep_duration(struct timespec *duration, struct timeval *wakeup) {
    struct timeval now;
    gettimeofday(&now, NULL);
    duration->tv_sec = wakeup->tv_sec - now.tv_sec;
    duration->tv_nsec = wakeup->tv_usec - now.tv_usec;
    
    // I think this while only ever runs once, but just in case use while
    while(duration->tv_nsec < 0) {
        duration->tv_sec -= 1;
        duration->tv_nsec += 1000000;
    }

    duration->tv_nsec *= 1000;

    return ((duration->tv_sec >= 0) && (duration->tv_nsec >= 0)) ? 0 : -1;
}

// Main function: entry point for execution
int main(int argc, char *argv[]) {
    struct timespec sleep_time;
    struct timeval next_wakeup;
    gettimeofday(&next_wakeup, NULL);

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
    time_t period_s = tArg / 1000;
    suseconds_t period_us = (tArg % 1000) * 1000;
    signal(SIGEXCESS, handle_sigexcess);


    // printf("Cost: %d ms (%lld clock cycles)\n", cArg, clocks_running);
    // printf("Period: %d ms (%d s %d us)\n", tArg, period_s, period_us);
    // printf("Core: %d\n", cpuArg);


    // Setting up CPU affinity using syscall for sched_setaffinity
    unsigned long cpumask = 1UL << cpuArg;  // Set affinity to specified CPU
    if (syscall(__NR_sched_setaffinity, 0, sizeof(cpumask), &cpumask) == -1) {
        perror("sched_setaffinity syscall");
        return -1;
    }


    while(1)
    {
        increment_by_period(&next_wakeup, period_s, period_us);
        clock_t start = clock();
        clock_t clocks_periodElapsedTime = 0;

        // printf("Started period...");
        // fflush(stdout);

        //pretend to do busy work
        while(clocks_periodElapsedTime < clocks_running)
        {
            clocks_periodElapsedTime = (clock() - start);
        }

        // printf("Ended period %d\n", (clock() / CLOCKS_PER_SEC));

        if (set_sleep_duration(&sleep_time, &next_wakeup) < 0) {
            printf("Overran period\n");
            return -1;
        }

        nanosleep(&sleep_time, NULL);
    }

    return 0;
}
