//
// I/O redirection hooks (bytehook) - PojavLauncher/ZalithLauncher.
//
// API bytehook verificada contra o cabecalho oficial bytehook.h 1.0.10
// (com.bytedance:bytehook) - nenhum simbolo inventado.
//

#include <jni.h>
#include <fcntl.h>
#include <cstdarg>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <android/log.h>
#include <bytehook.h>

#define LOG_TAG "IORedirectHook"
#include "logger/logger.h"

// Assinatura original do open() da libc.
typedef int (*orig_open_t)(const char *pathname, int flags, ...);

// Resolvido ANTES de instalar o hook. Nunca fica NULL: se dlsym falhar,
// init_io_hooks() aborta a instalacao em vez de deixar o hook a chamar NULL.
static orig_open_t orig_open = nullptr;

// Hook da funcao open() da libc.
static int hooked_open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (mode_t)va_arg(args, int);
        va_end(args);
    }

    // O endereco original vem do dlsym (RTLD_DEFAULT) e nao da stack do bytehook,
    // por isso e sempre valido e nunca reentra neste hook.
    if (orig_open == nullptr) {
        return -1;
    }
    int fd = orig_open(pathname, flags, mode);

    // Obrigatorio em modo AUTOMATIC: desempilha a entrada colocada pelo trampoline.
    BYTEHOOK_POP_STACK();

    // Se for um ficheiro de chunk (.mca) ou config critica, damos a dica ao kernel.
    if (fd >= 0 && pathname != nullptr) {
        if (strstr(pathname, ".mca") != nullptr || strstr(pathname, "level.dat") != nullptr) {
            posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
            LOGI("Optimizing I/O for: %s", pathname);
        }
    }

    return fd;
}

// Inicializacao dos hooks. Chamado via ZLBridge.initIoHooks() -> utils.c.
extern "C" void init_io_hooks() {
    LOGI("Initializing I/O redirection hooks...");

    // 1. Resolver o open() original primeiro - se falhar, nao instalar nada.
    orig_open = (orig_open_t)dlsym(RTLD_DEFAULT, "open");
    if (orig_open == nullptr) {
        LOGE("Cannot resolve origin open(): %s", dlerror());
        return;
    }

    // 2. Mesmo padrao usado em exit_hook.c (mesma versao do bytehook).
    int status = bytehook_init(BYTEHOOK_MODE_AUTOMATIC, false);
    if (status != BYTEHOOK_STATUS_CODE_OK) {
        LOGE("bytehook_init failed (%d)", status);
        return;
    }

    // 3. Assinatura real: bytehook_hook_all(callee_path_name, sym_name,
    //    new_func, hooked, hooked_arg) - 5 argumentos.
    bytehook_stub_t stub = bytehook_hook_all(
        nullptr,
        "open",
        reinterpret_cast<void *>(hooked_open),
        nullptr,
        nullptr);
    LOGI("I/O hooks installed, stub = %p", stub);
}
