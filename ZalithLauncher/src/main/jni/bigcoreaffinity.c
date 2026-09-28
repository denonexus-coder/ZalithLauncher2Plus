//
// Created by maks on 19.06.2023.
//

#define _GNU_SOURCE // we are GNU GPLv3

#include <linux/limits.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#define FREQ_MAX 256
#define MAX_CPU 32

static unsigned long core_freqs[MAX_CPU];
static unsigned core_count = 0;
static unsigned long g_max_freq = 0;

void bigcore_format_cpu_path(char* buffer, unsigned int cpu_core) {
    snprintf(buffer, PATH_MAX, "/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_max_freq", cpu_core);
}

void bigcore_set_affinity(void) {
    char path_buffer[PATH_MAX];
    char freq_buffer[FREQ_MAX];
    char* discard;
    g_max_freq = 0;
    core_count = 0;
    while (core_count < MAX_CPU) {
        snprintf(path_buffer, PATH_MAX,
                 "/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_max_freq", core_count);
        int fd = open(path_buffer, O_RDONLY);
        if (fd == -1) break;
        ssize_t rc = read(fd, freq_buffer, FREQ_MAX - 1);
        close(fd);
        if (rc <= 0) break;
        freq_buffer[rc] = 0;
        core_freqs[core_count] = strtoul(freq_buffer, &discard, 10);
        if (core_freqs[core_count] > g_max_freq) g_max_freq = core_freqs[core_count];
        core_count++;
    }
    if (core_count == 0 || g_max_freq == 0) {
        printf("bigcore: no cpufreq info, affinity not set\n");
        return;
    }
    cpu_set_t mask;
    CPU_ZERO(&mask);
    unsigned pinned = 0;
    for (unsigned i = 0; i < core_count; i++) {
        if (core_freqs[i] >= g_max_freq * 80 / 100) { CPU_SET(i, &mask); pinned++; }
    }
    if (pinned == 0) { CPU_SET(core_count - 1, &mask); pinned = 1; }
    printf("bigcore: pinning to %u big core(s)\n", pinned);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0)
        printf("bigcore: failed: %s\n", strerror(errno));
}

void bigcore_apply_to_all_threads(void) {
    if (core_count == 0 || g_max_freq == 0) return;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    unsigned pinned = 0;
    for (unsigned i = 0; i < core_count; i++) {
        if (core_freqs[i] >= g_max_freq * 80 / 100) { CPU_SET(i, &mask); pinned++; }
    }
    if (pinned == 0) { CPU_SET(core_count - 1, &mask); pinned = 1; }
    DIR* d = opendir("/proc/self/task");
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        pid_t tid = atoi(e->d_name);
        if (tid > 0) sched_setaffinity(tid, sizeof(cpu_set_t), &mask);
    }
    closedir(d);
    printf("bigcore: applied to all threads (%u cores)\n", pinned);
}
