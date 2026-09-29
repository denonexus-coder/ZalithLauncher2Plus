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

package com.movtery.zalithlauncher.ui.screens.game.elements

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.Crossfade
import androidx.compose.animation.expandIn
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkOut
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.focusProperties
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.movtery.zalithlauncher.R
import com.movtery.zalithlauncher.game.recorder.RecordingState
import com.movtery.zalithlauncher.ui.components.FloatingBall
import com.movtery.zalithlauncher.ui.screens.game.PerfStats

@Composable
fun DraggableGameBall(
    position: Offset,
    onPositionChanged: (Offset) -> Unit,
    onSavePos: () -> Unit,
    stats: PerfStats?,
    sparkline: List<PerfStats>,
    opened: Boolean,
    alpha: Float = 1f,
    onClick: () -> Unit = {},
    recordingState: RecordingState = RecordingState.IDLE,
    elapsedMs: Long = 0L,
    micEnabled: Boolean = false,
    onPauseRecording: () -> Unit = {},
    onResumeRecording: () -> Unit = {},
    onStopRecording: () -> Unit = {},
    onToggleMic: () -> Unit = {}
) {
    val isRecordingActive = recordingState == RecordingState.RECORDING ||
            recordingState == RecordingState.PAUSED

    FloatingBall(
        modifier = Modifier.focusProperties {
            canFocus = false
        },
        position = position,
        onPositionChanged = onPositionChanged,
        onSavePos = onSavePos,
        onClick = onClick,
        alpha = alpha
    ) {
        GameBallContent(
            stats = stats,
            sparkline = sparkline,
            opened = opened,
            isRecordingActive = isRecordingActive,
            isPaused = recordingState == RecordingState.PAUSED,
            elapsedMs = elapsedMs,
            micEnabled = micEnabled,
            onPauseRecording = onPauseRecording,
            onResumeRecording = onResumeRecording,
            onStopRecording = onStopRecording,
            onToggleMic = onToggleMic,
        )
    }
}

@Composable
private fun RecordingControlContent(
    isPaused: Boolean,
    elapsedMs: Long,
    micEnabled: Boolean,
    onPause: () -> Unit,
    onResume: () -> Unit,
    onStop: () -> Unit,
    onToggleMic: () -> Unit
) {
    Row(
        modifier = Modifier.padding(horizontal = 4.dp, vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            painter = painterResource(R.drawable.ic_fiber_manual_record),
            contentDescription = null,
            modifier = Modifier.size(12.dp),
            tint = if (isPaused) MaterialTheme.colorScheme.onSurface.copy(alpha = 0.4f)
                   else Color.Red
        )
        Spacer(Modifier.width(3.dp))
        Text(
            text = elapsedMs.formatElapsedTime(),
            style = MaterialTheme.typography.labelSmall,
        )
        IconButton(
            onClick = onToggleMic,
            modifier = Modifier.size(28.dp)
        ) {
            Icon(
                painter = painterResource(
                    if (micEnabled) R.drawable.ic_mic
                    else R.drawable.ic_mic_off
                ),
                contentDescription = stringResource(
                    if (micEnabled) R.string.recorder_mic_on else R.string.recorder_mic_off
                ),
                modifier = Modifier.size(18.dp),
                tint = if (micEnabled) MaterialTheme.colorScheme.primary
                       else MaterialTheme.colorScheme.onSurface
            )
        }
        IconButton(
            onClick = if (isPaused) onResume else onPause,
            modifier = Modifier.size(28.dp)
        ) {
            Icon(
                painter = painterResource(
                    if (isPaused) R.drawable.ic_play_arrow_filled
                    else R.drawable.ic_pause_filled
                ),
                contentDescription = stringResource(
                    if (isPaused) R.string.recorder_resume else R.string.recorder_pause
                ),
                modifier = Modifier.size(18.dp)
            )
        }
        IconButton(
            onClick = onStop,
            modifier = Modifier.size(28.dp)
        ) {
            Icon(
                painter = painterResource(R.drawable.ic_stop_filled),
                contentDescription = stringResource(R.string.recorder_stop_and_save),
                modifier = Modifier.size(18.dp),
                tint = MaterialTheme.colorScheme.error
            )
        }
    }
}

private fun Long.formatElapsedTime(): String {
    val totalSeconds = this / 1000L
    val hours = totalSeconds / 3600
    val minutes = (totalSeconds % 3600) / 60
    val seconds = totalSeconds % 60
    return if (hours > 0) {
        "%02d:%02d:%02d".format(hours, minutes, seconds)
    } else {
        "%02d:%02d".format(minutes, seconds)
    }
}

@Composable
private fun PerformancePanel(stats: PerfStats, sparkline: List<PerfStats>) {
    val numericStyle = MaterialTheme.typography.labelSmall.copy(fontFeatureSettings = "tnum")
    val path = remember { Path() }
    val summary = remember { StringBuilder() }
    val maxRam = stats.heapMaxMb.coerceAtLeast(1)
    val ramProgress = (stats.heapUsedMb.toFloat() / maxRam).coerceIn(0f, 1f)
    val peakFps = (sparkline.maxOfOrNull { it.fps } ?: stats.fps).coerceAtLeast(1).toFloat()
    val fpsColor = fpsBandColor(stats.fps)
    val frameColor = frameTimeBandColor(stats.frametimeMs)
    val ramColor = ramBandColor(ramProgress)
    summary.setLength(0)
    summary.append("avg ").append(stats.avg.toInt())
        .append(" · min ").append(stats.min)
        .append(" · max ").append(stats.max)

    Column(
        modifier = Modifier
            .background(Color.Black.copy(alpha = 0.6f), RoundedCornerShape(8.dp))
            .padding(horizontal = 6.dp, vertical = 4.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("FPS ${stats.fps}", color = fpsColor, style = numericStyle)
            Canvas(Modifier.width(48.dp).height(16.dp)) {
                if (sparkline.size > 1) {
                    path.reset()
                    sparkline.forEachIndexed { index, point ->
                        val x = size.width * index / (sparkline.size - 1)
                        val y = size.height - (point.fps / peakFps * size.height).coerceIn(0f, size.height)
                        if (index == 0) path.moveTo(x, y) else path.lineTo(x, y)
                    }
                    drawPath(path, color = fpsColor, style = Stroke(width = 2f))
                }
            }
            Spacer(Modifier.width(4.dp))
            Text(
                summary.toString(),
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                style = numericStyle
            )
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(formatTenths(stats.frametimeMs) + "ms", color = frameColor, style = numericStyle)
            Spacer(Modifier.width(6.dp))
            Text("1% low ${stats.low1}", color = fpsColor, style = numericStyle)
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("RAM ${stats.heapUsedMb}M", color = ramColor, style = numericStyle)
            Spacer(Modifier.width(5.dp))
            LinearProgressIndicator(
                progress = { ramProgress },
                modifier = Modifier.width(54.dp).height(4.dp),
                color = ramColor,
                trackColor = Color.White.copy(alpha = 0.18f)
            )
            Spacer(Modifier.width(5.dp))
            Text(
                "máx ${stats.heapMaxMb}M",
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                style = numericStyle
            )
        }
    }
}

private fun fpsBandColor(fps: Int) = when {
    fps >= 50 -> Color(0xFF4ADE80)
    fps >= 30 -> Color(0xFFFACC15)
    else -> Color(0xFFF87171)
}

private fun frameTimeBandColor(ms: Float) = when {
    ms <= 20f -> Color(0xFF4ADE80)
    ms <= 33f -> Color(0xFFFACC15)
    else -> Color(0xFFF87171)
}

private fun ramBandColor(progress: Float) = when {
    progress < 0.70f -> Color(0xFF4ADE80)
    progress <= 0.90f -> Color(0xFFFACC15)
    else -> Color(0xFFF87171)
}

private fun formatTenths(value: Float): String {
    val tenths = (value * 10f).toInt().coerceAtLeast(0)
    return "${tenths / 10}.${tenths % 10}"
}

@Composable
private fun GameBallContent(
    stats: PerfStats?,
    sparkline: List<PerfStats>,
    opened: Boolean,
    isRecordingActive: Boolean = false,
    isPaused: Boolean = false,
    elapsedMs: Long = 0L,
    micEnabled: Boolean = false,
    onPauseRecording: () -> Unit = {},
    onResumeRecording: () -> Unit = {},
    onStopRecording: () -> Unit = {},
    onToggleMic: () -> Unit = {},
) {
    Row(
        modifier = Modifier.padding(all = 2.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(modifier = Modifier.size(28.dp), contentAlignment = Alignment.Center) {
            Crossfade(opened) { state ->
                Icon(
                    modifier = Modifier.size(24.dp),
                    painter = painterResource(if (state) R.drawable.ic_menu_open else R.drawable.ic_menu),
                    contentDescription = null
                )
            }
        }

        // Painel de performance: só show/hide anima, os valores atualizam sem animação
        if (stats != null) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Spacer(Modifier.width(4.dp))
                PerformancePanel(stats, sparkline)
            }
        }

        AnimatedVisibility(
            visible = isRecordingActive,
            enter = expandIn(expandFrom = Alignment.CenterStart) + fadeIn(),
            exit = shrinkOut(shrinkTowards = Alignment.CenterStart) + fadeOut()
        ) {
            RecordingControlContent(
                isPaused = isPaused,
                elapsedMs = elapsedMs,
                micEnabled = micEnabled,
                onPause = onPauseRecording,
                onResume = onResumeRecording,
                onStop = onStopRecording,
                onToggleMic = onToggleMic
            )
        }
    }
}
