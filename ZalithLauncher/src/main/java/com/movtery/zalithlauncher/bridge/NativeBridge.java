package com.movtery.zalithlauncher.bridge;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class NativeBridge {
    // Buffer direto off-heap: os eventos nunca sao objetos Java, por isso
    // nao ha alocação nem pressão de GC durante o arrasto.
    // Cada evento ocupa 20 bytes (5 campos int32/float) -> 102 cabem em 2048,
    // por isso o lote fecha aos 100.
    private static final int FLUSH_THRESHOLD = 100;

    private static final ByteBuffer inputBuffer = ByteBuffer.allocateDirect(2048).order(ByteOrder.nativeOrder());
    private static int eventCount = 0;

    static {
        // As funcoes nativas desta classe vivem em libpojavexec.so.
        // "zl_native" nunca existiu -> o System.loadLibrary falhava silenciosamente
        // e a resolucao dos simbolos ficava dependente da ordem de inicializacao.
        try {
            System.loadLibrary("pojavexec");
        } catch (UnsatisfiedLinkError e) {
            // Ja carregada pelo ZLBridge - ignorar.
        }
    }

    private static native void sendEventsFast(ByteBuffer buffer, int count);
    public static native void bindCurrentThreadToBigCores();

    // Constantes alinhadas com EVENT_TYPE_* de input_bridge_v3.c / ZLBridge.java.
    public static final int EVENT_TYPE_CHAR = 1000;
    public static final int EVENT_TYPE_CURSOR_POS = 1003;
    public static final int EVENT_TYPE_KEY = 1005;
    public static final int EVENT_TYPE_MOUSE_BUTTON = 1006;
    public static final int EVENT_TYPE_SCROLL = 1007;

    // Accoes GLFW: usadas pelo critical_send_key / critical_send_mouse_button.
    public static final int ACTION_RELEASE = 0;
    public static final int ACTION_PRESS = 1;
    public static final int ACTION_REPEAT = 2;

    // Botao esquerdo, na numeracao GLFW (0 = esquerdo, 1 = direito, 2 = meio).
    public static final int BUTTON_LEFT = 0;

    /**
     * Acrescenta um evento ao lote. Sem "action" o jogo nao conseguia
     * distinguir premido de solto - era preciso um evento por transicao JNI.
     */
    public static synchronized void queueInput(int type, int keyOrButton, int action, float x, float y) {
        if (eventCount >= FLUSH_THRESHOLD) flushEvents();

        inputBuffer.putInt(type);
        inputBuffer.putInt(keyOrButton);
        inputBuffer.putInt(action);
        inputBuffer.putFloat(x);
        inputBuffer.putFloat(y);
        eventCount++;
    }

    /**
     * Envia o lote acumulado com uma unica chamada JNI.
     */
    public static synchronized void flushEvents() {
        if (eventCount == 0) return;
        inputBuffer.flip();
        sendEventsFast(inputBuffer, eventCount);
        inputBuffer.clear();
        eventCount = 0;
    }
}
