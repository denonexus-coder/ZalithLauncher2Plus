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

// Pontos de entrada do pipeline de input, ja existentes em input_bridge_v3.c
// (compilado como C, logo com linkage C). Sao exatamente as mesmas funcoes
// que recebem ZLBridge.sendKey / sendMousePress / sendMousePos - ou seja,
// este lote cai no mesmo sitio a que chega o input normal do jogo.
extern "C" {
    void critical_send_key(int key, int scancode, int action, int mods);
    void critical_send_mouse_button(int button, int action, int mods);
    void critical_send_cursor_pos(float x, float y);
    void critical_send_scroll(double xoffset, double yoffset);
}

// Constantes de evento: tem de bater certo com EVENT_TYPE_* de
// input_bridge_v3.c e com ZLBridge.java. CURSOR_POS (1003) so existe em
// Java, porque no nativo o cursor e tratado diretamente pela funcao.
#define EVENT_TYPE_CHAR          1000
#define EVENT_TYPE_CURSOR_POS    1003
#define EVENT_TYPE_KEY           1005
#define EVENT_TYPE_MOUSE_BUTTON  1006
#define EVENT_TYPE_SCROLL        1007

// Limite por chamada. O buffer do Java tem 2048 bytes e cada evento ocupa
// 20 bytes -> 102 cabem; Java fecha o lote aos 100. Um limite impede que
// um eventCount corrompido faca o loop ler alem do buffer direto.
#define MAX_EVENTS_PER_CALL 1024

// Estrutura compacta para eventos (20 bytes).
// Sem o campo "action" nao ha como distinguir tecla premida de tecla solta,
// o que tornava o lote completamente inutil para o jogo.
struct __attribute__((packed)) InputEvent {
    int32_t type;
    int32_t key_or_button;
    int32_t action;   // 0 solta, 1 premida, 2 repetida
    float   x;
    float   y;
};

// Injeta um lote de inputs numa unica transicao JNI (zero-copy): em vez de
// uma chamada JNI por evento, faz-se uma so para o lote inteiro e depois
// despacha-se tudo em C.
extern "C" JNIEXPORT void JNICALL
Java_com_movtery_zalithlauncher_bridge_NativeBridge_sendEventsFast(JNIEnv* env, jclass clazz, jobject byteBuffer, jint eventCount) {
    (void) clazz;

    if (eventCount <= 0 || eventCount > MAX_EVENTS_PER_CALL) return;

    InputEvent* events = static_cast<InputEvent*>(env->GetDirectBufferAddress(byteBuffer));
    if (events == nullptr) {
        LOGE("sendEventsFast: direct buffer address is NULL");
        return;
    }

    for (jint i = 0; i < eventCount; ++i) {
        const InputEvent& ev = events[i];
        switch (ev.type) {
            case EVENT_TYPE_KEY:
                critical_send_key(ev.key_or_button, 0, ev.action, 0);
                break;
            case EVENT_TYPE_MOUSE_BUTTON:
                critical_send_mouse_button(ev.key_or_button, ev.action, 0);
                break;
            case EVENT_TYPE_CURSOR_POS:
                critical_send_cursor_pos(ev.x, ev.y);
                break;
            case EVENT_TYPE_SCROLL:
                critical_send_scroll((double) ev.x, (double) ev.y);
                break;
            default:
                // Um tipo desconhecido nao pode morrer em silencio: indica que
                // Java e nativo andaram a usar constantes diferentes.
                LOGE("sendEventsFast: unknown event type %d (skipped)", (int) ev.type);
                break;
        }
    }
}

// Função para vincular a thread atual aos núcleos rápidos (já usada no bigcoreaffinity, mas exposta aqui para Java)
extern "C" JNIEXPORT void JNICALL
Java_com_movtery_zalithlauncher_bridge_NativeBridge_bindCurrentThreadToBigCores(JNIEnv* env, jclass clazz) {
    (void) env;
    (void) clazz;
    bigcore_apply_to_render_thread();
    LOGI("Java thread requested binding to BIG cores.");
}
