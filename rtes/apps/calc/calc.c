#include <stdio.h>
#include <linux/unistd.h>
#include <sys/syscall.h>

#include "calc.h"

int main(int argc, char **argv) {

    char buf[12] = {};
    int i;

    if (argc != 4) {
	printf("Use the form A (OP) B. (Used %d args)\n", argc);
	for (i = 0; i < argc; i++) {
	    printf("Arg %d: %s\n", i, argv[i]);
	}
	return -1;
    }

    int x = calc(argv[1], argv[3], *argv[2], buf);
    if (x) {
	printf("nan\n");
    } else {
	printf("%s\n", buf);
    }
}

