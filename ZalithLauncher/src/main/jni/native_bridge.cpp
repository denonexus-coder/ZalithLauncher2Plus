#include <jni.h>
#include <stdint.h>
#include <android/log.h>
#include <sched.h>
#include <unistd.h>

#define LOG_TAG "NativeBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

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
    extern void bigcore_apply_to_render_thread(void);
    bigcore_apply_to_render_thread();
    LOGI("Java thread requested binding to BIG cores.");
}
