/*
 * Zalith Launcher 2
 * Copyright (C) 2025 MovTery <movtery228@qq.com> and contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>.
 */

package com.movtery.zalithlauncher.bridge;

import android.content.Context;

import androidx.annotation.Keep;

@Keep
public final class ZLBridge {
    //AWT
    public static final int EVENT_TYPE_CHAR = 1000;
    public static final int EVENT_TYPE_CURSOR_POS = 1003;
    public static final int EVENT_TYPE_KEY = 1005;
    public static final int EVENT_TYPE_MOUSE_BUTTON = 1006;

    public static void sendKey(char keychar, int keycode) {
        // TODO: Android -> AWT keycode mapping
        sendInputData(EVENT_TYPE_KEY, (int) keychar, keycode, 1, 0);
        sendInputData(EVENT_TYPE_KEY, (int) keychar, keycode, 0, 0);
    }

    public static void sendKey(char keychar, int keycode, int state) {
        // TODO: Android -> AWT keycode mapping
        sendInputData(EVENT_TYPE_KEY, (int) keychar, keycode, state, 0);
    }

    public static void sendChar(char keychar){
        sendInputData(EVENT_TYPE_CHAR, (int) keychar, 0, 0, 0);
    }

    public static void sendMousePress(int awtButtons, boolean isDown) {
        sendInputData(EVENT_TYPE_MOUSE_BUTTON, awtButtons, isDown ? 1 : 0, 0, 0);
    }

    public static void sendMousePress(int awtButtons) {
        sendMousePress(awtButtons, true);
        sendMousePress(awtButtons, false);
    }

    public static void sendMousePos(int x, int y) {
        sendInputData(EVENT_TYPE_CURSOR_POS, x, y, 0, 0);
    }

    //Game
    @Keep public static native void initializeGameExitHook();
    public static native void initIoHooks();
    @Keep public static native void setupExitMethod(Context context);
    public static native void initIoHooks();

    //Launch
    @Keep public static native void setLdLibraryPath(String ldLibraryPath);
    public static native void initIoHooks();
    @Keep public static native boolean dlopen(String libPath);
    public static native void initIoHooks();

    //Render
    @Keep public static native void setupBridgeWindow(Object surface);
    public static native void initIoHooks();
    @Keep public static native void releaseBridgeWindow();
    public static native void initIoHooks();
    @Keep public static native void moveWindow(int xOffset, int yOffset);
    public static native void initIoHooks();
    @Keep public static native int[] renderAWTScreenFrame();
    public static native void initIoHooks();

    //Input
    @Keep public static native void sendInputData(int type, int i1, int i2, int i3, int i4);
    public static native void initIoHooks();
    @Keep public static native void clipboardReceived(String data, String mimeTypeSub);
    public static native void initIoHooks();

    //Utils
    @Keep public static native int chdir(String path);
    public static native void initIoHooks();
    @Keep public static native void fsrInit(int qualityPreset);
    public static native void initIoHooks();
    @Keep public static native void fsrSetQuality(int qualityPreset);
    public static native void initIoHooks();
    @Keep public static native void fpsLimitSet(int fps);
    public static native void initIoHooks();
    @Keep public static native void registerStatsBuffer(java.nio.ByteBuffer buf);
    public static native void initIoHooks();

    static {
        NativeLibraryLoader.loadExitHookLib();
        NativeLibraryLoader.loadPojavLib();
        NativeLibraryLoader.loadPojavAWTLib();

        //Android 14'te native_handle_create libnativewindow.so'ya taşındı.
        //Ana çözüm: java_exec_hooks.c ile FFmpeg subprocess'inin LD_PRELOAD'ına
        //libnativewindow.so eklendi. Ayrıca burada RTLD_GLOBAL ile in-process
        //yüklemelere karşı ek güvence sağlıyoruz. RTLD_LOCAL kullanılmaz.
        NativeLibraryLoader.reloadFFmpegSystemDependenciesGlobally();
    }
}
