#include <jni.h>
#include <assert.h>
#include <dlfcn.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GL/osmesa.h>
#include "ctxbridges/egl_loader.h"
#include "ctxbridges/osmesa_loader.h"
#include "ctxbridges/renderer_config.h"
#include "ctxbridges/virgl_bridge.h"
#include "driver_helper/nsbypass.h"

#ifdef GLES_TEST
#include <GLES2/gl2.h>
#endif

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/rect.h>
#include <string.h>
#include <environ/environ.h>
#include <android/dlext.h>
#include <time.h>
#include <stdatomic.h>
#include "utils.h"
#include "ctxbridges/bridge_tbl.h"
#include "ctxbridges/osm_bridge.h"

#define GLFW_CLIENT_API 0x22001
/* Consider GLFW_NO_API as Vulkan API */
#define GLFW_NO_API 0
#define GLFW_OPENGL_API 0x30001

// This means that the function is an external API and that it will be used
#define EXTERNAL_API __attribute__((used))
// This means that you are forced to have this function/variable for ABI compatibility
#define ABI_COMPAT __attribute__((unused))

EGLConfig config;
struct PotatoBridge potatoBridge;

void* loadTurnipVulkan(const char* driver_path, const char* native_dir, const char* cache_dir);
void calculateFPS();

EXTERNAL_API void pojavTerminate() {
    printf("EGLBridge: Terminating\n");

    switch (pojav_environ->config_renderer) {
        case RENDERER_GL4ES: {
            eglMakeCurrent_p(potatoBridge.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroySurface_p(potatoBridge.eglDisplay, potatoBridge.eglSurface);
            eglDestroyContext_p(potatoBridge.eglDisplay, potatoBridge.eglContext);
            eglTerminate_p(potatoBridge.eglDisplay);
            eglReleaseThread_p();

            potatoBridge.eglContext = EGL_NO_CONTEXT;
            potatoBridge.eglDisplay = EGL_NO_DISPLAY;
            potatoBridge.eglSurface = EGL_NO_SURFACE;
        } break;

            //case RENDERER_VIRGL:
        case RENDERER_VK_ZINK: {
            // Nothing to do here
        } break;
    }
}

JNIEXPORT void JNICALL Java_com_movtery_zalithlauncher_bridge_ZLBridge_setupBridgeWindow(JNIEnv* env, ABI_COMPAT jclass clazz, jobject surface) {
    pojav_environ->pojavWindow = ANativeWindow_fromSurface(env, surface);
    if (br_setup_window) br_setup_window();
}

JNIEXPORT void JNICALL
Java_com_movtery_zalithlauncher_bridge_ZLBridge_releaseBridgeWindow(ABI_COMPAT JNIEnv *env, ABI_COMPAT jclass clazz) {
    ANativeWindow_release(pojav_environ->pojavWindow);
}

EXTERNAL_API void* pojavGetCurrentContext() {
    if (pojav_environ->config_renderer == RENDERER_VIRGL)
        return virglGetCurrentContext();

    return br_get_current();
}

static void set_vulkan_ptr(void* ptr) {
    char envval[64];
    sprintf(envval, "%"PRIxPTR, (uintptr_t)ptr);
    setenv("VULKAN_PTR", envval, 1);
}

void load_vulkan() {
    const char* zinkPreferSystemDriver = getenv("POJAV_ZINK_PREFER_SYSTEM_DRIVER");
    int deviceApiLevel = android_get_device_api_level();
    if (zinkPreferSystemDriver == NULL && deviceApiLevel >= 28) {
#ifdef ADRENO_POSSIBLE
        const char* native_dir = getenv("DRIVER_PATH");
        const char* cache_dir = getenv("TMPDIR");

        void* result = loadTurnipVulkan(NULL, native_dir, cache_dir);
        if (result != NULL)
        {
            printf("AdrenoSupp: Loaded Turnip, loader address: %p\n", result);
            set_vulkan_ptr(result);
            return;
        }
#endif
    }

    printf("OSMDroid: Loading Vulkan regularly...\n");
    void* vulkanPtr = dlopen("libvulkan.so", RTLD_LAZY | RTLD_GLOBAL);
    printf("OSMDroid: Loaded Vulkan, ptr=%p\n", vulkanPtr);
    set_vulkan_ptr(vulkanPtr);
}

int pojavInitOpenGL() {
    const char *renderer = getenv("POJAV_RENDERER");

    if (!strncmp("opengles", renderer, 8) || !strcmp(renderer, "mobileglues"))
    {
        pojav_environ->config_renderer = RENDERER_GL4ES;
        if (!strcmp(renderer, "opengles3_desktopgl_zink_kopper")) {
            load_vulkan();
            setenv("GALLIUM_DRIVER", "zink", 1);
            setenv("MESA_ANDROID_NO_KMS_SWRAST", "1", 1);
        }
        set_gl_bridge_tbl();
    }

    if (!strcmp(renderer, "custom_gallium"))
    {
        pojav_environ->config_renderer = RENDERER_VK_ZINK;
        load_vulkan();
        set_osm_bridge_tbl();
    }

    if (!strcmp(renderer, "vulkan_zink"))
    {
        pojav_environ->config_renderer = RENDERER_VK_ZINK;
        load_vulkan();
        setenv("GALLIUM_DRIVER", "zink", 1);
        set_osm_bridge_tbl();
    }

    if (!strcmp(renderer, "gallium_freedreno"))
    {
        pojav_environ->config_renderer = RENDERER_VK_ZINK;
        load_vulkan();
        setenv("MESA_LOADER_DRIVER_OVERRIDE", "kgsl", 1);
        setenv("GALLIUM_DRIVER", "freedreno", 1);
        set_osm_bridge_tbl();
    }

    if (!strcmp(renderer, "gallium_panfrost"))
    {
        pojav_environ->config_renderer = RENDERER_VK_ZINK;
        setenv("GALLIUM_DRIVER", "panfrost", 1);
        setenv("MESA_DISK_CACHE_SINGLE_FILE", "1", 1);
        set_osm_bridge_tbl();
    }

    if (!strcmp(renderer, "gallium_virgl"))
    {
        pojav_environ->config_renderer = RENDERER_VIRGL;
        setenv("GALLIUM_DRIVER", "virpipe", 1);
        setenv("OSMESA_NO_FLUSH_FRONTBUFFER", "1", false);
        setenv("MESA_GL_VERSION_OVERRIDE", "4.3", 1);
        setenv("MESA_GLSL_VERSION_OVERRIDE", "430", 1);
        if (!strcmp(getenv("OSMESA_NO_FLUSH_FRONTBUFFER"), "1"))
            printf("VirGL: OSMesa buffer flush is DISABLED!\n");
        loadSymbolsVirGL();
        virglInit();
        return 0;
    }

    if (br_init()) br_setup_window();

    return 0;
}

EXTERNAL_API int pojavInit() {
    ANativeWindow_acquire(pojav_environ->pojavWindow);
    pojav_environ->savedWidth = ANativeWindow_getWidth(pojav_environ->pojavWindow);
    pojav_environ->savedHeight = ANativeWindow_getHeight(pojav_environ->pojavWindow);
    ANativeWindow_setBuffersGeometry(pojav_environ->pojavWindow,pojav_environ->savedWidth,pojav_environ->savedHeight,AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM);
    pojavInitOpenGL();
    return 1;
}

EXTERNAL_API void pojavSetWindowHint(int hint, int value) {
    if (hint != GLFW_CLIENT_API) return;
    switch (value) {
        case GLFW_NO_API:
            pojav_environ->config_renderer = RENDERER_VULKAN;
            /* Nothing to do: initialization is handled in Java-side */
            // pojavInitVulkan();
            break;
        case GLFW_OPENGL_API: {
            const char *renderer = getenv("POJAV_RENDERER");
            if (!strncmp("opengles", renderer, 8) || !strcmp(renderer, "mobileglues")) {
                pojav_environ->config_renderer = RENDERER_GL4ES;
            } else if (!strcmp(renderer, "vulkan_zink")) {
                pojav_environ->config_renderer = RENDERER_VK_ZINK;
            }
            /* Nothing to do: initialization is called in pojavCreateContext */
            // pojavInitOpenGL();
            break;
        }
        default:
            printf("GLFW: Unimplemented API 0x%x\n", value);
            abort();
    }
}

EXTERNAL_API void pojavSwapBuffers() {
    calculateFPS();

    if (pojav_environ->config_renderer == RENDERER_VK_ZINK
     || pojav_environ->config_renderer == RENDERER_GL4ES)
    {
        br_swap_buffers();
    }

    if (pojav_environ->config_renderer == RENDERER_VIRGL)
    {
        virglSwapBuffers();
    }

}

EXTERNAL_API void pojavMakeCurrent(void* window) {
    if (pojav_environ->config_renderer == RENDERER_VK_ZINK
     || pojav_environ->config_renderer == RENDERER_GL4ES)
    {
        br_make_current((basic_render_window_t*)window);
    }

    if (pojav_environ->config_renderer == RENDERER_VIRGL)
    {
        virglMakeCurrent(window);
    }

}

EXTERNAL_API void* pojavCreateContext(void* contextSrc) {
    if (pojav_environ->config_renderer == RENDERER_VULKAN)
        return (void *) pojav_environ->pojavWindow;

    if (pojav_environ->config_renderer == RENDERER_VIRGL)
        return virglCreateContext(contextSrc);

    return br_init_context((basic_render_window_t*)contextSrc);
}

void* maybe_load_vulkan() {
    // We use the env var because
    // 1. it's easier to do that
    // 2. it won't break if something will try to load vulkan and osmesa simultaneously
    if(getenv("VULKAN_PTR") == NULL) load_vulkan();
    return (void*) strtoul(getenv("VULKAN_PTR"), NULL, 0x10);
}

struct zl_stats {
    uint32_t frame_deltas_us[256];
    uint64_t last_frame_ns;
    uint64_t next_publish_ns;
    uint32_t ring_idx;
    uint32_t sample_count;
    int32_t fps, fps_min, fps_max, low1;
    float fps_avg, frametime_ms;
};
static struct zl_stats g_stats = {0};
static int32_t* g_statsShared = NULL;

static int cmp_u32(const void* a, const void* b) {
    const uint32_t left = *(const uint32_t*)a;
    const uint32_t right = *(const uint32_t*)b;
    return left < right ? -1 : (left > right ? 1 : 0);
}

static void zl_store_shared_float(int32_t* slot, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    __atomic_store_n((uint32_t*)slot, bits, __ATOMIC_RELAXED);
}

static void zl_publish(struct zl_stats* s) {
    uint32_t count = s->sample_count;
    if (count == 0) {
        s->fps = 0; s->fps_min = 0; s->fps_max = 0; s->low1 = 0;
        s->fps_avg = 0.0f; s->frametime_ms = 0.0f;
        return;
    }
    if (count > 256) count = 256;
    uint32_t sorted[256];
    memcpy(sorted, s->frame_deltas_us, count * sizeof(uint32_t));
    qsort(sorted, count, sizeof(uint32_t), cmp_u32);
    uint32_t min_delta = sorted[0];
    uint32_t max_delta = sorted[count - 1];
    uint64_t total_delta = 0;
    uint32_t valid_frames = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (sorted[i] == 0) continue;
        total_delta += sorted[i];
        valid_frames++;
    }
    if (valid_frames == 0 || total_delta == 0) {
        s->fps = 0; s->fps_min = 0; s->fps_max = 0; s->low1 = 0;
        s->fps_avg = 0.0f; s->frametime_ms = 0.0f;
        return;
    }
    const uint32_t p99_base = (count * 99) / 100;
    const uint32_t p99_index = p99_base + 1 < count ? p99_base + 1 : count - 1;
    const uint32_t p99_delta = sorted[p99_index];
    s->fps = 0;
    for (uint32_t i = 0; i < count; i++) if (sorted[i] > 0 && sorted[i] <= 1000000U) s->fps++;
    s->fps_min = max_delta > 0 ? (int32_t)(1000000U / max_delta) : 0;
    s->fps_max = min_delta > 0 ? (int32_t)(1000000U / min_delta) : 0;
    s->fps_avg = 1000000.0f / ((float)total_delta / (float)valid_frames);
    s->frametime_ms = (float)total_delta / ((float)valid_frames * 1000.0f);
    s->low1 = p99_delta > 0 ? (int32_t)(1000000U / p99_delta) : 0;
    s->sample_count = 0; s->ring_idx = 0;
    if (g_statsShared != NULL) {
        __atomic_store_n(g_statsShared + 0, s->fps, __ATOMIC_RELAXED);
        __atomic_store_n(g_statsShared + 1, s->fps_min, __ATOMIC_RELAXED);
        __atomic_store_n(g_statsShared + 2, s->fps_max, __ATOMIC_RELAXED);
        __atomic_store_n(g_statsShared + 3, s->low1, __ATOMIC_RELAXED);
        zl_store_shared_float(g_statsShared + 4, s->fps_avg);
        zl_store_shared_float(g_statsShared + 5, s->frametime_ms);
    }
}

void calculateFPS() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    const uint64_t now = (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
    if (g_stats.last_frame_ns != 0) {
        const uint64_t elapsed = now - g_stats.last_frame_ns;
        const uint32_t delta_us = elapsed > 0 ? (uint32_t)(elapsed / 1000ULL) : 0;
        g_stats.frame_deltas_us[g_stats.ring_idx++ & 255U] = delta_us;
        if (g_stats.sample_count < 256U) g_stats.sample_count++;
    }
    g_stats.last_frame_ns = now;
    if (now >= g_stats.next_publish_ns) {
        g_stats.next_publish_ns = now + 1000000000ULL;
        zl_publish(&g_stats);
    }

    if (!pojav_environ->hasGraphicOutput && pojav_environ->dalvikJavaVMPtr && pojav_environ->bridgeClazz && pojav_environ->method_onGraphicOutput) {
        pojav_environ->hasGraphicOutput = true;
        JNIEnv *dalvikEnv;
        (*pojav_environ->dalvikJavaVMPtr)->AttachCurrentThread(pojav_environ->dalvikJavaVMPtr, &dalvikEnv, NULL);
        (*dalvikEnv)->CallStaticVoidMethod(dalvikEnv, pojav_environ->bridgeClazz, pojav_environ->method_onGraphicOutput);
        (*pojav_environ->dalvikJavaVMPtr)->DetachCurrentThread(pojav_environ->dalvikJavaVMPtr);
    }
}

EXTERNAL_API JNIEXPORT void JNICALL
Java_com_movtery_zalithlauncher_bridge_ZLBridge_registerStatsBuffer(JNIEnv* env, jclass clazz, jobject buf) {
    (void)clazz;
    g_statsShared = (int32_t*)(*env)->GetDirectBufferAddress(env, buf);
}

EXTERNAL_API JNIEXPORT void JNICALL
Java_org_lwjgl_vulkan_VK_onVKFrame(ABI_COMPAT JNIEnv *env, ABI_COMPAT jclass thiz) {
    calculateFPS();
}

EXTERNAL_API JNIEXPORT jint JNICALL
Java_org_lwjgl_glfw_CallbackBridge_getCurrentFps(JNIEnv *env, jclass clazz) {
    (void)env; (void)clazz;
    return g_stats.fps;
}

EXTERNAL_API JNIEXPORT jlong JNICALL
Java_org_lwjgl_vulkan_VK_getVulkanDriverHandle(ABI_COMPAT JNIEnv *env, ABI_COMPAT jclass thiz) {
    printf("EGLBridge: LWJGL-side Vulkan loader requested the Vulkan handle\n");
    return (jlong) maybe_load_vulkan();
}
EXTERNAL_API void pojavSwapInterval(int interval) {
    if (pojav_environ->config_renderer == RENDERER_VK_ZINK
     || pojav_environ->config_renderer == RENDERER_GL4ES)
    {
        br_swap_interval(interval);
    }

    if (pojav_environ->config_renderer == RENDERER_VIRGL)
    {
        virglSwapInterval(interval);
    }

}

