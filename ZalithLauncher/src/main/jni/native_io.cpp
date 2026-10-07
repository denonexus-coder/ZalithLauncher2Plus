#include <jni.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "NativeIO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Escrita ultra-rápida usando mmap + memcpy direto
extern "C" JNIEXPORT jboolean JNICALL
Java_com_movtery_zalithlauncher_bridge_NativeIO_fastWrite(JNIEnv* env, jclass clazz, 
                                                           jstring filePath, jobject byteBuffer, jint dataSize) {
    const char* path = env->GetStringUTFChars(filePath, nullptr);
    if (!path) return JNI_FALSE;

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    env->ReleaseStringUTFChars(filePath, path);
    
    if (fd < 0) {
        LOGE("Failed to open file for writing");
        return JNI_FALSE;
    }

    // Redimensionar ficheiro antes de mapear
    if (ftruncate(fd, dataSize) != 0) {
        close(fd);
        return JNI_FALSE;
    }

    // Mapear ficheiro na memória virtual
    void* map = mmap(nullptr, dataSize, PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        LOGE("mmap failed for write");
        close(fd);
        return JNI_FALSE;
    }

    // Guarda de overflow: nunca escrever mais do que a capacidade real do buffer.
    jlong capacity = env->GetDirectBufferCapacity(byteBuffer);
    if (capacity >= 0 && dataSize > capacity) {
        LOGE("fastWrite: dataSize %d > buffer capacity %lld", (int)dataSize, (long long)capacity);
        munmap(map, dataSize);
        close(fd);
        return JNI_FALSE;
    }

    // Cópia direta Zero-Copy do buffer Java para o ficheiro mapeado
    void* bufferData = env->GetDirectBufferAddress(byteBuffer);
    if (bufferData) {
        std::memcpy(map, bufferData, dataSize);
        msync(map, dataSize, MS_SYNC); // Forçar escrita no disco
    }

    munmap(map, dataSize);
    close(fd);
    return JNI_TRUE;
}

// Leitura ultra-rápida usando mmap
extern "C" JNIEXPORT jint JNICALL
Java_com_movtery_zalithlauncher_bridge_NativeIO_fastRead(JNIEnv* env, jclass clazz, 
                                                          jstring filePath, jobject byteBuffer) {
    const char* path = env->GetStringUTFChars(filePath, nullptr);
    if (!path) return -1;

    int fd = open(path, O_RDONLY);
    env->ReleaseStringUTFChars(filePath, path);
    
    if (fd < 0) return -1;

    struct stat sb;
    if (fstat(fd, &sb) == -1 || sb.st_size == 0) {
        close(fd);
        return -1;
    }

    jlong fileSize = sb.st_size;
    void* map = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    
    if (map == MAP_FAILED) {
        close(fd);
        return -1;
    }

    // Dica ao kernel: acesso sequencial (otimiza read-ahead para chunks)
    posix_fadvise(fd, 0, fileSize, POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);

    void* bufferData = env->GetDirectBufferAddress(byteBuffer);
    if (!bufferData) {
        munmap(map, fileSize);
        close(fd);
        return -1;
    }

    // Guarda de overflow: o buffer de destino e limitado (4MB). Sem esta checagem
    // um ficheiro maior causaria memory corruption nativa.
    jlong capacity = env->GetDirectBufferCapacity(byteBuffer);
    if (capacity < 0 || fileSize > capacity) {
        LOGE("fastRead: file size %lld > buffer capacity %lld", (long long)fileSize, (long long)capacity);
        munmap(map, fileSize);
        close(fd);
        return -1;
    }

    std::memcpy(bufferData, map, fileSize);

    munmap(map, fileSize);
    close(fd);
    return (jint)fileSize;
}
