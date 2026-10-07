package com.movtery.zalithlauncher.game.support.input

import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.pointer.PointerInputChange
import androidx.compose.ui.input.pointer.PointerInputScope
import com.movtery.zalithlauncher.bridge.NativeBridge

/**
 * Gestor de inputs de alta performance que utiliza a ponte Zero-Copy.
 * Elimina a alocação de objetos durante o arrasto (drag) para evitar GC stutter.
 */
object OptimizedInputHandler {

    // Constantes para tipos de input (devem corresponder ao struct em C++)
    private const val TYPE_KEYBOARD = 0
    private const val TYPE_MOUSE_CLICK = 1
    private const val TYPE_MOUSE_MOVE = 2

    /**
     * Detecta gestos de arrasto e envia as coordenadas via NativeBridge.
     * Usa detectDragGestures para obter o histórico de movimentos.
     */
    fun PointerInputScope.detectOptimizedDrag(
        onDragStart: (Offset) -> Unit = {},
        onDragEnd: () -> Unit = {},
        onDragCancel: () -> Unit = {},
        onDrag: (change: PointerInputChange, dragAmount: Offset) -> Unit
    ) {
        detectDragGestures(
            onDragStart = { offset -> 
                onDragStart(offset)
                // Opcional: Enviar evento de "clique inicial" se necessário
            },
            onDragEnd = { 
                onDragEnd()
                NativeBridge.flushEvents() // Garante que todos os movimentos são enviados
            },
            onDragCancel = { 
                onDragCancel()
                NativeBridge.flushEvents()
            },
            onDrag = { change, dragAmount ->
                // Consumir o evento para evitar propagação desnecessária
                change.consume()
                
                // Enviar movimento para a ponte nativa
                NativeBridge.queueInput(
                    type = TYPE_MOUSE_MOVE,
                    keyOrButton = 0,
                    x = change.position.x,
                    y = change.position.y
                )
                
                // Chamar callback original se necessário para lógica interna
                onDrag(change, dragAmount)
            }
        )
    }

    /**
     * Detecta toques simples e envia como cliques de rato.
     */
    fun PointerInputScope.detectOptimizedTap(
        onLongPress: (Offset) -> Unit = {},
        onTap: (Offset) -> Unit
    ) {
        detectTapGestures(
            onLongPress = onLongPress,
            onTap = { offset ->
                // Enviar clique esquerdo (button 1)
                NativeBridge.queueInput(TYPE_MOUSE_CLICK, 1, offset.x, offset.y)
                NativeBridge.flushEvents()
                onTap(offset)
            }
        )
    }
}
