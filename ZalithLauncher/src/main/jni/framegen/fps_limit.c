#include "fps_limit.h"

#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <android/log.h>

#define LOG_TAG "FPSLimit"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static int g_targetFps = 0;
static struct timespec g_lastSwap = {0, 0};
static bool g_initialized = false;

void fpslimit_set_limit(int fps) {
    g_targetFps = fps;
    g_initialized = false;
}

int fpslimit_get_limit(void) {
    return g_targetFps;
}

void fpslimit_throttle(void) {
    if (g_targetFps <= 0) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (!g_initialized) {
        g_lastSwap = now;
        g_initialized = true;
        return;
    }

    long targetNs = 1000000000L / g_targetFps;

    long elapsedNs = (now.tv_sec - g_lastSwap.tv_sec) * 1000000000L
                   + (now.tv_nsec - g_lastSwap.tv_nsec);

    if (elapsedNs < targetNs) {
        struct timespec deadline;
        deadline.tv_sec = g_lastSwap.tv_sec;
        deadline.tv_nsec = g_lastSwap.tv_nsec + targetNs;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1000000000L;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
        clock_gettime(CLOCK_MONOTONIC, &g_lastSwap);
    } else {
        g_lastSwap = now;
    }
}
