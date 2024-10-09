#include <signal.h>
#include <stdbool.h> 
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define MAX_PROCESSES 1024
#define MAX_TASK_NAME 16

struct process_info {
	int32_t pid;
	int32_t tid;
	int32_t rt_priority;
	char name[MAX_TASK_NAME+1];
};


void hide_cursor() {
	printf("\033[?251"); // ANSI escape code to hide cursor
}

void show_cursor() {
	printf("\033[?25h"); // ANSI escape code to show cursor
}

void reset_cursor() {
	printf("\033[H"); // ANSI escape code to move cursor to top-left corner
}

void clean_exit(int sig) {
	printf("\033[0m"); // Reset terminal colors
	system("clear");
	show_cursor();
	exit(0);
}

int main() {
	struct process_info pinfo[MAX_PROCESSES];
	int32_t ret, max_seen, i;

	signal(SIGINT, clean_exit);

	system("clear");
	hide_cursor();

	while(true) {
		ret = syscall(__NR_list_rt_threads, &pinfo, MAX_PROCESSES);

		if (ret < 0) 
			clean_exit(ret);
		if (ret < max_seen)
			max_seen = ret;

		reset_cursor();

		printf("PID\t\tTID\t\tRT_PRIORITY\tNAME\n");
		printf("-------------------------------------------------------------------------\n");

		for (i = 0; i < ret; i++) {
			printf("% 10d\t% 10d\t\% 10d\t%s\n", 
			  	pinfo[i].pid, 
			  	pinfo[i].tid, 
			  	pinfo[i].rt_priority,
			  	pinfo[i].name
			);
		}

		for (i = ret; i < max_seen; i++) 
			printf("\33[2K\r\n");

		hide_cursor();

		sleep(2);
	}

	return 0;
}

