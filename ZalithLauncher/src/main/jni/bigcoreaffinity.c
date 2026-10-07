#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sched.h>
#include <android/log.h>
#include <pthread.h>
#include <errno.h>

#define LOG_TAG "BigCoreAffinity"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Máscara de CPU para os cores BIG do MT6765 (cores 0-3)
// Bitmask: 0b00001111 = 0x0F
#define BIG_CORE_MASK 0x0F
#define LITTLE_CORE_MASK 0xF0

static cpu_set_t g_big_core_set;
static cpu_set_t g_little_core_set;
static int g_initialized = 0;
static int g_big_core_count = 0;
static int g_little_core_count = 0;

/**
 * Detecta dinamicamente quais cores são BIG baseando-se na frequência máxima.
 * No MT6765: cores 0-3 @ 2.3GHz = BIG, cores 4-7 @ 1.8GHz = LITTLE
 */
static void detect_cpu_topology(void) {
    CPU_ZERO(&g_big_core_set);
    CPU_ZERO(&g_little_core_set);
    
    int max_freq_khz = 0;
    int core_freqs[16] = {0};
    int num_cores = 0;
    
    // Ler frequência máxima de cada core
    for (int i = 0; i < 16; i++) {
        char path[128];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
        
        FILE *f = fopen(path, "r");
        if (!f) break;
        
        int freq = 0;
        if (fscanf(f, "%d", &freq) == 1) {
            core_freqs[i] = freq;
            if (freq > max_freq_khz) max_freq_khz = freq;
            num_cores = i + 1;
        }
        fclose(f);
    }
    
    if (num_cores == 0 || max_freq_khz == 0) {
        LOGW("Failed to detect CPU topology, using default mask 0x%02X", BIG_CORE_MASK);
        // Fallback para MT6765
        for (int i = 0; i < 4; i++) CPU_SET(i, &g_big_core_set);
        for (int i = 4; i < 8; i++) CPU_SET(i, &g_little_core_set);
        g_big_core_count = 4;
        g_little_core_count = 4;
        g_initialized = 1;
        return;
    }
    
    // Threshold: cores com frequência >= 90% da máxima são considerados BIG
    int threshold = (max_freq_khz * 90) / 100;
    
    for (int i = 0; i < num_cores; i++) {
        if (core_freqs[i] >= threshold) {
            CPU_SET(i, &g_big_core_set);
            g_big_core_count++;
            LOGI("Core %d: %d kHz -> BIG", i, core_freqs[i]);
        } else {
            CPU_SET(i, &g_little_core_set);
            g_little_core_count++;
            LOGI("Core %d: %d kHz -> LITTLE", i, core_freqs[i]);
        }
    }
    
    g_initialized = 1;
    LOGI("CPU Topology: %d BIG cores, %d LITTLE cores", g_big_core_count, g_little_core_count);
}

/**
 * Aplica afinidade de BIG cores apenas à thread atual (Render Thread).
 * NÃO aplica a todas as threads - isso causa stutter por competição com GC.
 */
void bigcore_apply_to_render_thread(void) {
    if (!g_initialized) detect_cpu_topology();
    
    pid_t tid = gettid();
    if (sched_setaffinity(tid, sizeof(g_big_core_set), &g_big_core_set) < 0) {
        LOGE("Failed to set render thread affinity to tid %d: %s", tid, strerror(errno));
    } else {
        LOGI("Render thread (tid %d) pinned to BIG cores (mask 0x%02X)", tid, BIG_CORE_MASK);
    }
}

/**
 * Aplica afinidade de LITTLE cores à thread atual (para GC, áudio, etc).
 */
void bigcore_apply_to_background_thread(void) {
    if (!g_initialized) detect_cpu_topology();
    
    pid_t tid = gettid();
    if (sched_setaffinity(tid, sizeof(g_little_core_set), &g_little_core_set) < 0) {
        LOGW("Failed to set background thread affinity to tid %d: %s", tid, strerror(errno));
    }
}

/**
 * Retorna a máscara de BIG cores para uso externo.
 */
cpu_set_t* bigcore_get_big_mask(void) {
    if (!g_initialized) detect_cpu_topology();
    return &g_big_core_set;
}

/**
 * Inicialização chamada pelo launcher no arranque.
 */
void bigcore_init(void) {
    detect_cpu_topology();
}
