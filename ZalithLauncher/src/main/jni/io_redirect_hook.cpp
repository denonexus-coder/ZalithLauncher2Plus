#include <jni.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <android/log.h>
#include <bytehook.h>

#define LOG_TAG "IORedirectHook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Ponteiros para as funções originais
typedef int (*orig_open_t)(const char *pathname, int flags, ...);
typedef ssize_t (*orig_read_t)(int fd, void *buf, size_t count);

static orig_open_t orig_open = nullptr;
static orig_read_t orig_read = nullptr;

// Cache simples para ficheiros mapeados (em produção usarias um std::unordered_map)
struct MappedFile {
    int fd;
    void* map;
    size_t size;
};

// Hook da função open() do Linux
static int hooked_open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (mode_t)va_arg(args, int);
        va_end(args);
    }

    // Chamar a função original primeiro
    int fd = orig_open(pathname, flags, mode);
    
    // Se for um ficheiro de chunk (.mca) ou config crítica, tentamos otimizar
    if (fd >= 0 && pathname != nullptr) {
        if (strstr(pathname, ".mca") != nullptr || strstr(pathname, "level.dat") != nullptr) {
            LOGI("Optimizing I/O for: %s", pathname);
            // Aqui poderíamos preparar o mmap imediatamente se quiséssemos
            // Por agora, apenas logamos para confirmar que o hook está ativo
        }
    }
    
    return fd;
}

// Inicialização dos hooks
extern "C" void init_io_hooks() {
    LOGI("Initializing I/O redirection hooks...");
    
    bytehook_init(BYTEHOOK_MODE_AUTOMATIC, false);
    
    // Hook na libc (open/read)
    bytehook_hook_all(
        nullptr, 
        "open", 
        reinterpret_cast<void*>(hooked_open), 
        reinterpret_cast<void**>(&orig_open), 
        nullptr
    );
    
    LOGI("I/O hooks installed successfully.");
}
