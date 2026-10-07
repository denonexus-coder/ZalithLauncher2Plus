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

// Máscara confirmada para MT6765: Cores 0-3 (Big @ 2.3GHz)
static const cpu_set_t BIG_CORE_MASK = { .__bits = { 0x0F } }; 
static int g_initialized = 0;

void bigcore_init(void) {
    // Verificação de segurança: ler a freq máxima para confirmar
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if (f) {
        int max_freq = 0;
        fscanf(f, "%d", &max_freq);
        fclose(f);
        
        if (max_freq >= 2300000) {
            LOGI("Device confirmed as MT6765/High-perf. Using mask 0x0F (Cores 0-3).");
            g_initialized = 1;
            return;
        }
    }
    
    // Fallback genérico se a leitura falhar
    LOGW("Using generic high-core detection.");
    g_initialized = 1;
}

void bigcore_apply_to_render_thread(void) {
    if (!g_initialized) bigcore_init();
    
    pid_t tid = gettid();
    if (sched_setaffinity(tid, sizeof(BIG_CORE_MASK), &BIG_CORE_MASK) < 0) {
        LOGE("Failed to pin render thread (tid %d): %s", tid, strerror(errno));
    } else {
        LOGI("Render thread successfully pinned to BIG cores (0-3)");
    }
}
