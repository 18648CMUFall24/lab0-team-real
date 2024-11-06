#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include "../reserve/reserve.h"
#include "../calc/calc.h"
#include <unistd.h>
#include <stdlib.h>

const char *DATA_PATH = "/sys/rtes/taskmon/util";
const char *ENABLE = "/sys/rtes/taskmon/enabled";

void do_reservation(void);
void cancel_reservation(void);
void monitor(void);

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
	// system("clear");
	show_cursor();
}

void clear_lines() {
	printf("\033[2J\n"); // Reset lines
}

int main(int argc, char *argv[]) {

	char mode, c;

	while (true) {
		printf("(r)eserve, (c)ancel reservation, (m)onitor, (q)uit\n> ");
		mode = getchar();
		while ((c = getchar()) != '\n' && c != EOF); // Ignore & clean STDIN 

		switch (mode) {
			case 'R':
			case 'r':
				do_reservation();
				break;
			case 'C':
			case 'c': 
				cancel_reservation();
				break;
			case 'M':
			case 'm':
				monitor();
				break;
			case 'Q':
			case 'q':
				return 0;
			default:
				printf("Invalid option\n");
		}

	}

	return 0;
}

void do_reservation() {
	struct timespec cost, period;

	int tid, C, T, cpuno;
	printf("Enter the following: tid cost(ms) period(ms) cpu\n> ");
	if (scanf(" %d %d %d %d", &tid, &C, &T, &cpuno) != 4) {
		printf("everything is stupid\n");
	}

	printf("Entered pid: %d, C: %d, T: %d, cpu: %d\n", tid, C, T, cpuno);

	if (cpuno > 3) { printf("Invalid cpu\n"); return; }
	if (C > T) { printf("Invalid cost & period\n"); return; }
	if (T > 60000) { printf("Period too long\n"); return; }


	// memcpy(&cost, 0, sizeof(struct timespec));
	cost.tv_sec = C / 1000;
	cost.tv_nsec = (C % 1000) * 1000000;
	printf("Cost %ld.%09ld\n", (long)cost.tv_sec, (long)cost.tv_nsec);

	// memcpy(&period, 0, sizeof(struct timespec));
	period.tv_sec = T / 1000;
	period.tv_nsec = (T % 1000) * 1000000;
	printf("Period %ld.%09ld\n", (long) period.tv_sec, (long) period.tv_nsec);

	set_reserve(tid, &period, &cost, cpuno);
}

void cancel_reservation() {
	int tid;
	printf("Enter id of thread to cancel reservation for: ");
	scanf(" %d", &tid);
	cancel_reserve(tid);
}

void monitor() {
	struct dirent *entry;
	char file_path[1024];
	DIR *dp;
	FILE *f;
	char count[10], value[10], acc[10];
	long timestamp;

	// set up signal handler or smth for sigint
	f = fopen(ENABLE, "w");
	if (f != NULL) {
		fputc('1', f);
		fclose(f);
	}

	while (true)  {
		dp = opendir(DATA_PATH);

		if (dp == NULL) {
			printf("Failed to open path\n");
			return;
		}

		clear_lines();
		reset_cursor();

		printf("TID\t\tAVERAGE_UTIL\n");
		printf("-------------------------------------------------------\n");

		entry = readdir(dp);
		while (entry != NULL) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
				entry = readdir(dp);
				continue;
			};

			snprintf(file_path, sizeof(file_path), "%s/%s", DATA_PATH, entry->d_name);

			FILE *file = fopen(file_path, "r");
			if (file == NULL) continue;

			memset(&count, '0', 10);
			strcpy((char*)&count, "0.0");

			memset(&value, '0', 10);
			strcpy((char*)&value, "0.0");

			memset(&acc, '0', 10);
			strcpy((char*)&acc, "0.0");

			while(fscanf(file, "%ld\t%s\n", &timestamp, (char*)&value) == 1) {
				calc((char*)&count, "1.0", '+', (char*)&count);
				calc((char*)&acc, (char*)&value, '+', (char*)&acc);
			}

			fclose(file);

			if (!strcmp("0.0", count)) {
				calc((char*)&acc, (char*)&count, '/', (char*)&value);
				printf("%s\t\t%s\n", entry->d_name, (char*)value);
			} 

			entry = readdir(dp);

		}

		hide_cursor();
		sleep(2);
	}


}

