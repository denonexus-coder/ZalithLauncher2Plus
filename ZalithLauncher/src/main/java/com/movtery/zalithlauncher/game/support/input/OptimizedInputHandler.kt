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

    // Constantes de evento: tem de corresponder a EVENT_TYPE_* de
    // input_bridge_v3.c (e nao a um enum ad-hoc, senao o nativo descarta tudo).
    private const val EVENT_TYPE_CURSOR_POS = 1003
    private const val EVENT_TYPE_MOUSE_BUTTON = 1006

    /**
     * Detecta gestos de arrasto e envia as coordenadas via NativeBridge.
     * Usa detectDragGestures para obter o histórico de movimentos.
     */
    // detectDragGestures/detectTapGestures sao suspend: os wrappers tambem
    // tem de ser, senao o compilador Kotlin recusa a chamada.
    suspend fun PointerInputScope.detectOptimizedDrag(
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
                
                // Enviar movimento para a ponte nativa.
                // queueInput() e um metodo Java: os argumentos tem de ser
                // posicionais (named arguments nao sao permitidos em Java).
                NativeBridge.queueInput(
                    EVENT_TYPE_CURSOR_POS,
                    0,
                    NativeBridge.ACTION_RELEASE,
                    change.position.x,
                    change.position.y
                )
                
                // Chamar callback original se necessário para lógica interna
                onDrag(change, dragAmount)
            }
        )
    }

    /**
     * Detecta toques simples e envia como cliques de rato.
     */
    suspend fun PointerInputScope.detectOptimizedTap(
        onLongPress: (Offset) -> Unit = {},
        onTap: (Offset) -> Unit
    ) {
        detectTapGestures(
            onLongPress = onLongPress,
            onTap = { offset ->
                // Um tap tem de virar clique completo: premir e soltar. Só o
                // premir deixava o botao preso no jogo ate ao proximo clique.
                NativeBridge.queueInput(
                    EVENT_TYPE_MOUSE_BUTTON,
                    NativeBridge.BUTTON_LEFT,
                    NativeBridge.ACTION_PRESS,
                    offset.x,
                    offset.y
                )
                NativeBridge.queueInput(
                    EVENT_TYPE_MOUSE_BUTTON,
                    NativeBridge.BUTTON_LEFT,
                    NativeBridge.ACTION_RELEASE,
                    offset.x,
                    offset.y
                )
                NativeBridge.flushEvents()
                onTap(offset)
            }
        )
    }
}
