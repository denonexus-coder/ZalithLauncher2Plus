package com.movtery.zalithlauncher.bridge;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class NativeBridge {
    // Buffer direto de 2KB para inputs (Off-Heap, zero GC pressure)
    private static final ByteBuffer inputBuffer = ByteBuffer.allocateDirect(2048).order(ByteOrder.nativeOrder());
    private static int eventCount = 0;

    static {
        try {
            System.loadLibrary("zl_native"); // Ou o nome da tua lib principal
        } catch (UnsatisfiedLinkError e) {
            // Fallback se a lib ainda não estiver compilada
        }
    }

    private static native void sendEventsFast(ByteBuffer buffer, int count);
    public static native void bindCurrentThreadToBigCores();

    public static synchronized void queueInput(int type, int keyOrButton, float x, float y) {
        if (eventCount >= 100) flushEvents();
        
        inputBuffer.putInt(type);
        inputBuffer.putInt(keyOrButton);
        inputBuffer.putFloat(x);
        inputBuffer.putFloat(y);
        eventCount++;
    }

    public static synchronized void flushEvents() {
        if (eventCount == 0) return;
        inputBuffer.flip();
        sendEventsFast(inputBuffer, eventCount);
        inputBuffer.clear();
        eventCount = 0;
    }
}
