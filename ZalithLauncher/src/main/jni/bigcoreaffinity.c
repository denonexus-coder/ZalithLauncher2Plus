// O NDK r25 so expoe cpu_set_t/CPU_SET*/sched_setaffinity com _GNU_SOURCE.
// Tem de ser a PRIMEIRA linha, antes de qualquer include.
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <dirent.h>
#include <android/log.h>

#define LOG_TAG "BigCoreAffinity"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

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
        long half = num_cores > 0 ? num_cores / 2 : 4;
        for (long i = half; i < num_cores && i < 16; i++) CPU_SET((int) i, &g_big_core_set);
        LOGW("Fallback: Using upper half cores.");
    }

    // Ultima garantia: um cpu_set_t vazio faz sched_setaffinity falhar com EINVAL.
    if (CPU_COUNT(&g_big_core_set) == 0) {
        CPU_SET(0, &g_big_core_set);
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

// ---------------------------------------------------------------------------
// As duas funcoes abaixo sao CHAMADAS por input_bridge_v3.c (JNI_OnLoad) quando
// POJAV_BIG_CORE_AFFINITY esta definido. Foram apagadas na reescrita anterior e
// deixavam o link do libpojavexec.so a falhar com "undefined reference".
// ---------------------------------------------------------------------------

// Pina a thread chamadora (o JNI_OnLoad corre na thread de bootstrap).
void bigcore_set_affinity(void) {
    if (!g_initialized) bigcore_init();

    if (sched_setaffinity(0, sizeof(g_big_core_set), &g_big_core_set) != 0) {
        LOGE("bigcore_set_affinity failed: %s", strerror(errno));
    } else {
        LOGI("bigcore: process pinned to %d BIG core(s)", CPU_COUNT(&g_big_core_set));
    }
}

// Pina todas as threads ja criadas pelo processo.
void bigcore_apply_to_all_threads(void) {
    if (!g_initialized) bigcore_init();

    DIR *d = opendir("/proc/self/task");
    if (!d) return;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        pid_t tid = (pid_t) atoi(e->d_name);
        if (tid > 0) {
            if (sched_setaffinity(tid, sizeof(g_big_core_set), &g_big_core_set) != 0) {
                LOGE("bigcore: failed to pin tid %d: %s", tid, strerror(errno));
            }
        }
    }
    closedir(d);
    LOGI("bigcore: applied to all threads (%d cores)", CPU_COUNT(&g_big_core_set));
}
