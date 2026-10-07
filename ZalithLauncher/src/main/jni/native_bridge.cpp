#include <jni.h>
#include <stdint.h>
#include <android/log.h>
#include <sched.h>
#include <unistd.h>

#define LOG_TAG "NativeBridge"
#include "logger/logger.h"

// Definida em bigcoreaffinity.c (compilado como C). Sem extern "C" o C++
// mangleia o nome e o linker falha com undefined symbol.
extern "C" void bigcore_apply_to_render_thread(void);

// Estrutura compacta para eventos (16 bytes)
struct __attribute__((packed)) InputEvent {
    int32_t type;       // 0: Teclado, 1: Rato, 2: Movimento
    int32_t key_or_button;
    float x;
    float y;
};

// Função nativa para injetar lotes de inputs (Zero-Copy)
extern "C" JNIEXPORT void JNICALL
Java_com_movtery_zalithlauncher_bridge_NativeBridge_sendEventsFast(JNIEnv* env, jclass clazz, jobject byteBuffer, jint eventCount) {
    InputEvent* events = static_cast<InputEvent*>(env->GetDirectBufferAddress(byteBuffer));
    if (!events) return;

    // Aqui iríamos chamar as funções internas da LWJGL/GLFW se tivéssemos acesso aos headers.
    // Por enquanto, isto serve como a estrutura base de alta performance.
    for (int i = 0; i < eventCount; ++i) {
        // Exemplo: _glfwInputKey(window, events[i].key_or_button, 0, 1, 0);
    }
}

// Função para vincular a thread atual aos núcleos rápidos (já usada no bigcoreaffinity, mas exposta aqui para Java)
extern "C" JNIEXPORT void JNICALL
Java_com_movtery_zalithlauncher_bridge_NativeBridge_bindCurrentThreadToBigCores(JNIEnv* env, jclass clazz) {
    bigcore_apply_to_render_thread();
    LOGI("Java thread requested binding to BIG cores.");
}
