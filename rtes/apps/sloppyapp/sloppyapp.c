#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <sys/stat.h>

int main() {
    // Set the name of the current task to "sloppy"
    // if (prctl(PR_SET_NAME, "sloppyapp", 0, 0, 0) != 0) {
    //     perror("prctl");
    //     return EXIT_FAILURE;
    // }

    // Print the current process ID and process name (comm)
    // char comm[16];
    // if (prctl(PR_GET_NAME, comm, 0, 0, 0) != 0) {
    //     perror("prctl (get)");
    //     return EXIT_FAILURE;
    // }

    //printf("Task name (comm): %s\n", comm);
    printf("Process ID: %d\n", getpid());
    int fd = open("testfile.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    int fd1 = open("another.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    printf("File opened with file descriptor: %d\n", fd);
    printf("File opened with file descriptor: %d\n", fd1);

    // Sleep for a while to allow observation of the process
    sleep(5);

    _exit(0);
}
