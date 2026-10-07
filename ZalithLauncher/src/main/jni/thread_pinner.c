// Forward declaration from bigcoreaffinity.c
extern void bigcore_apply_to_render_thread(void);
extern void bigcore_init(void);
#define _GNU_SOURCE
#include <sched.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "logger/logger.h"

#define MASK_RENDER  (1u<<0)
#define MASK_SERVER  (1u<<1)
#define MASK_CHUNKS  (1u<<2)
#define MASK_FALLBACK ((1u<<3)|(1u<<4)|(1u<<5)|(1u<<6)|(1u<<7))
#define MAX_CPU 32

static _Atomic bool g_enabled = false;
static _Atomic int g_errorCount = 0;

static void set_mask(pid_t tid, unsigned m) {
    cpu_set_t s; CPU_ZERO(&s);
    for (int i = 0; i < MAX_CPU; i++) if (m & (1u << i)) CPU_SET(i, &s);
    if (sched_setaffinity(tid, sizeof(s), &s) < 0) { g_errorCount++; LOGE("sched_setaffinity failed for tid %d", tid); }
}

static unsigned pick(const char *n) {
    if (!strcmp(n, "Render thread")) return MASK_RENDER;
    if (!strcmp(n, "Server thread")) return MASK_SERVER;
    if (!strncmp(n, "Worker-Main", 11)) return MASK_CHUNKS;
    if (!strncmp(n, "Chunk Renderer", 14)) return MASK_CHUNKS;
    return MASK_FALLBACK;
}

static void *watch(void *a) {
    pthread_setname_np(pthread_self(), "ThreadPinner");
    pid_t self_tid = gettid();
    int logged_render = 0, logged_server = 0;
    while (g_enabled) {
        if (g_errorCount > 50) { g_enabled = false;
            printf("ZLithCpuPinner: too many affinity errors, disabling\n"); break; }
        DIR *d = opendir("/proc/self/task");
        if (!d) { usleep(500000); continue; }
        struct dirent *e;
        while ((e = readdir(d))) {
            pid_t tid = atoi(e->d_name);
            if (tid <= 0 || tid == self_tid) continue;
            char p[96], n[32] = "";
            snprintf(p, sizeof p, "/proc/self/task/%d/comm", tid);
            FILE *f = fopen(p, "r"); if (!f) continue;
            if (fgets(n, sizeof n, f)) n[strcspn(n, "\n")] = 0;
            fclose(f);
            unsigned m = pick(n);
            if (!strcmp(n, "Render thread") && !logged_render) {
                logged_render = 1;
                printf("ZLithCpuPinner: Render thread (tid=%d) pinned to core 0\n", tid);
            }
            if (!strcmp(n, "Server thread") && !logged_server) {
                logged_server = 1;
                printf("ZLithCpuPinner: Server thread (tid=%d) pinned to core 1\n", tid);
            }
            set_mask(tid, m);
        }
        closedir(d);
        usleep(500000);
    }
    return NULL;
}

void start_cpu_pinner(void) {
    static bool started = false;
    if (started) return;
    started = true;
    g_enabled = true;
    pthread_t t;
    pthread_create(&t, NULL, watch, NULL);
    pthread_detach(t);
    printf("ZLithCpuPinner: experimental CPU affinity layer started\n");
}
