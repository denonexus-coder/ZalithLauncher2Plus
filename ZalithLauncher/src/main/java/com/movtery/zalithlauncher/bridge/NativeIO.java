package com.movtery.zalithlauncher.bridge;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Interface de I/O de alta performance usando Memory-Mapped Files.
 * Evita o overhead de FileInputStream/FileOutputStream e reduz GC pressure.
 */
public class NativeIO {
    // Buffer reutilizável de 4MB para operações de I/O (Off-Heap)
    private static final ByteBuffer ioBuffer = ByteBuffer.allocateDirect(4 * 1024 * 1024)
            .order(ByteOrder.nativeOrder());

    static {
        try {
            System.loadLibrary("pojavexec");
        } catch (UnsatisfiedLinkError ignored) {}
    }

    private static native boolean fastWrite(String path, ByteBuffer buffer, int size);
    private static native int fastRead(String path, ByteBuffer buffer);

    /**
     * Escreve dados num ficheiro usando mmap.
     * @return true se a escrita foi bem-sucedida
     */
    public static synchronized boolean writeFile(File file, byte[] data) {
        if (data.length > ioBuffer.capacity()) {
            throw new IllegalArgumentException("Data exceeds 4MB buffer limit");
        }
        
        ioBuffer.clear();
        ioBuffer.put(data);
        return fastWrite(file.getAbsolutePath(), ioBuffer, data.length);
    }

    /**
     * Lê um ficheiro inteiro para memória usando mmap.
     * @return Os bytes lidos, ou null se falhar
     */
    public static synchronized byte[] readFile(File file) {
        ioBuffer.clear();
        int bytesRead = fastRead(file.getAbsolutePath(), ioBuffer);
        
        if (bytesRead <= 0) return null;

        byte[] result = new byte[bytesRead];
        ioBuffer.position(0);
        ioBuffer.get(result);
        return result;
    }
}
