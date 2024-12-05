#include <linux/unistd.h>
#include <sys/syscall.h>

int end_job() {
    return syscall(__NR_end_job);
}

