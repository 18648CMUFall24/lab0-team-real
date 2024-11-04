#include <linux/unistd.h>
#include <sys/syscall.h>

int calc(char *a, char *b, char op, char *out) {
    return syscall(__NR_calc, a, b, op, out);
}

