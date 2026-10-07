#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <android/log.h>

#define LOG_TAG "FSRHook"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/*
 * FSR hook — versão corrigida.
 * IMPORTANTE: apenas a API pública é exportada da libzl_fsr.so.
 * Os wrappers GL (glBindFramebuffer/glViewport/glGetIntegerv) NÃO são
 * declarados aqui e ficam com visibilidade escondida no fsr_hook.cpp.
 * Antes, estas declarações extern "C" exportavam os wrappers e, por a lib
 * ser carregada antes do renderer (Krypton Wrapper), intercetavam os
 * símbolos GL resolvidos pelo renderer/LWJGL → chamada a ponteiro nulo
 * quando o FSR não estava inicializado → SIGSEGV na inicialização.
 */

#if defined(__GNUC__)
#define FSR_API __attribute__((visibility("default")))
#else
#define FSR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Public API — consumida pelo launcher (JNI) e pelo gl_bridge.c (dlsym) */
FSR_API void fsr_init(int qualityPreset);
FSR_API void fsr_apply();
FSR_API void fsr_set_quality(int qualityPreset);
FSR_API void fsr_destroy();
/* Consulta rápida: o FSR está ativo? */
FSR_API int fsr_query_active(void);

/* Hook de eglGetProcAddress instalado via bytehook (chamado por
 * ponteiro direto, por isso TEM de estar exportado). Os wrappers GL
 * que devolve ficam escondidos dentro da lib. */
FSR_API void* hook_eglGetProcAddress(const char* procname);

#ifdef __cplusplus
}
#endif
