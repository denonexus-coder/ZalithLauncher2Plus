//
// Created by movte on 2025/4/25.
//

#ifndef ZALITHLAUNCHER_LOGGER_H
#define ZALITHLAUNCHER_LOGGER_H

#include <android/log.h>

// LOG_TAG pode (e deve) ser definido pelo ficheiro antes de incluir este header.
#ifndef LOG_TAG
#define LOG_TAG "ZL"
#endif

// Guardas #ifndef: ficheiros que ja definem LOGE (ex.: fsr_hook.h, bigcoreaffinity.c)
// continuam a compilar sem "macro redefined".
#ifndef LOGE
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif
#ifndef LOGW
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#endif
#ifndef LOGI
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#endif
#ifndef LOGD
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#endif

#define LOG_E "ERROR"
#define LOG_W "WARN"
#define LOG_I "INFO"
#define LOG_D "DEBUG"

#define LOG_TO_E(...) zl_log(LOG_E, __VA_ARGS__)
#define LOG_TO_W(...) zl_log(LOG_W, __VA_ARGS__)
#define LOG_TO_I(...) zl_log(LOG_I, __VA_ARGS__)
#define LOG_TO_D(...) zl_log(LOG_D, __VA_ARGS__)

// logger.c e compilado como C; sem este guarda os TUs C++ mangleiam o nome
// e o linker falharia com undefined symbol.
#ifdef __cplusplus
extern "C" {
#endif

void zl_log(const char *level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // ZALITHLAUNCHER_LOGGER_H