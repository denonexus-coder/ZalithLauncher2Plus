#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <android/log.h>

#define LOG_TAG "FSRHook"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#if defined(__GNUC__)
#define FSR_API __attribute__((visibility("default")))
#else
#define FSR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Public API - consumida pelo launcher (JNI) e pelo gl_bridge.c (dlsym) */
FSR_API void fsr_init(int qualityPreset);
FSR_API void fsr_apply();
FSR_API void fsr_set_quality(int qualityPreset);
FSR_API void fsr_destroy();
FSR_API int fsr_query_active(void);

/* Hook de eglGetProcAddress - exportado porque é chamado por ponteiro direto */
FSR_API void* hook_eglGetProcAddress(const char* procname);

#ifdef __cplusplus
}
#endif
