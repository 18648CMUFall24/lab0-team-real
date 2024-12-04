#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SYSFS_FREQ_PATH "/sys/rtes/freq"
#define SYSFS_POWER_PATH "/sys/rtes/power"
#define SYSFS_ENERGY_PATH "/sys/rtes/energy"
#define SYSFS_ENERGY_TID_PATH_FMT "/sys/rtes/tasks/%d/energy"

void read_sysfs_value(const char *path, long *value) {
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Failed to open sysfs file");
        exit(EXIT_FAILURE);
    }
    if (fscanf(file, "%ld", value) != 1) {
        perror("Failed to read value from sysfs file");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <tid>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int tid = atoi(argv[1]);
    char energy_tid_path[256];
    long freq_khz = 0, power_mw = 0, energy_mj = 0;

    if (tid < 0) {
        fprintf(stderr, "Invalid tid: %d\n", tid);
        return EXIT_FAILURE;
    }

    if (tid > 0) {
        snprintf(energy_tid_path, sizeof(energy_tid_path), SYSFS_ENERGY_TID_PATH_FMT, tid);
		printf("Energy path is %s\n",energy_tid_path);
    }

    printf("FREQ (MHZ)\tPOWER (mW)\tENERGY (mJ)\n");

    while (1) {
        // Read CPU frequency (kHz)
        read_sysfs_value(SYSFS_FREQ_PATH, &freq_khz);

        // Read power consumption (mW)
        read_sysfs_value(SYSFS_POWER_PATH, &power_mw);

        // Read energy consumption (mJ)
        if (tid > 0) {
            read_sysfs_value(energy_tid_path, &energy_mj);
        } else {
            read_sysfs_value(SYSFS_ENERGY_PATH, &energy_mj);
        }

        // Print the values in the required format
        printf("%lu\t\t%lu\t\t%lu\n", freq_khz, power_mw, energy_mj);

        // Wait for 1 second
        sleep(1);
    }

    return EXIT_SUCCESS;
}
