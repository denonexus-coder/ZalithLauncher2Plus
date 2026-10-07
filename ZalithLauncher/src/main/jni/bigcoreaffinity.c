#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "BigCoreAffinity"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static cpu_set_t g_big_core_set;
static int g_initialized = 0;

void bigcore_init(void) {
    CPU_ZERO(&g_big_core_set);
    
    long num_cores = sysconf(_SC_NPROCESSORS_CONF);
    long max_freqs[16] = {0};
    long absolute_max = 0;

    // 1. Ler frequências máximas de todos os núcleos
    for (int i = 0; i < num_cores && i < 16; i++) {
        char path[128];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
        FILE *f = fopen(path, "r");
        if (f) {
            fscanf(f, "%ld", &max_freqs[i]);
            fclose(f);
            if (max_freqs[i] > absolute_max) absolute_max = max_freqs[i];
        }
    }

    // 2. Selecionar núcleos que operam a >= 90% da frequência máxima
    long threshold = (absolute_max * 90) / 100;
    for (int i = 0; i < num_cores && i < 16; i++) {
        if (max_freqs[i] >= threshold) {
            CPU_SET(i, &g_big_core_set);
            LOGI("Core %d identified as BIG (%ld kHz)", i, max_freqs[i]);
        }
    }

    // Fallback se nada for encontrado ou se a leitura falhar
    if (CPU_COUNT(&g_big_core_set) == 0) {
        for (int i = num_cores / 2; i < num_cores; i++) CPU_SET(i, &g_big_core_set);
        LOGW("Fallback: Using upper half cores.");
    }
    
    g_initialized = 1;
}

void bigcore_apply_to_render_thread(void) {
    if (!g_initialized) bigcore_init();
    
    pid_t tid = gettid();
    if (sched_setaffinity(tid, sizeof(g_big_core_set), &g_big_core_set) < 0) {
        LOGE("Failed to pin render thread (tid %d): %s", tid, strerror(errno));
    } else {
        LOGI("Render thread pinned to %d BIG cores", CPU_COUNT(&g_big_core_set));
    }
}
