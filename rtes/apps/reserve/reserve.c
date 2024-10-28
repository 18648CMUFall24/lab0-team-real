
// Header file for input output functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <time.h>

// Main function: entry point for execution
int main(int argc, char *argv[]) 
{
    int returnSys;

    // check to see if there is at least 3 arguments
    if (argc < 3) {
        printf("There are less arguments!\n");
        return -1;
    }

    const char *syscallParam = argv[1];
    int threadID = atoi(argv[2]);

    if(strcmp(syscallParam, "set") == 0)
    {
        //Check to see if there is 6 arguments
        if (argc != 6)
        {
            printf("Not the right amount of arguments!");
            return -1;
        }
        
        //getting the arguemnts out
        struct timespec T = { .tv_sec = atoi(argv[3]) / 1000, .tv_nsec = ((atoi(argv[3])%1000) *1000000)};
        struct timespec C = { .tv_sec = atoi(argv[4]) / 1000, .tv_nsec = ((atoi(argv[4])%1000) *1000000)};
        int cpu = atoi(argv[5]);

        returnSys = syscall(__NR_set_reserve,threadID,T,C,cpu);
        if(returnSys < 0)
        {
            printf("Error in syscall!\n");
        }

        return 0;

    }
    else if(strcmp(syscallParam, "cancel") == 0)
    {
        //Check to see if there is 3 arguments
        if (argc != 3)
        {
            printf("Not the right amount of arguments!");
            return -1;
        }

        returnSys = syscall(__NR_cancel_reserve,threadID);

        if(returnSys < 0)
        {
            printf("Error in syscall!\n");
            return -1;
        }


        return 0;

    }
    else
    {
        printf("Not the right command!\n");
        return -1;
    }
}
