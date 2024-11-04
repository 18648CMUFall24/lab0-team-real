#include <sys/syscall.h>
#include <time.h>

int set_reserve(int threadID, struct timespec *T, struct timespec *C, int cpu) {
	return syscall(__NR_set_reserve, threadID, T, C, cpu);
}

int cancel_reserve(int threadID) {
        return syscall(__NR_cancel_reserve, threadID);
}

