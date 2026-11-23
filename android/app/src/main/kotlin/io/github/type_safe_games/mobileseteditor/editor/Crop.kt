// Mobile Set Editor - crop a picked image to a field's shape, with pan and pinch (#63)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.ImageDecoder
import android.net.Uri
import android.os.Build
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import java.io.File
import java.util.UUID
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Where an image sits under a crop frame of [frameW] x [frameH]: drawn at [scale] (frame pixels
 * per image pixel) with its top-left at ([x], [y]) in the frame. The image always covers the frame.
 */
data class CropState(val imageW: Int, val imageH: Int, val frameW: Float, val frameH: Float, val scale: Float, val x: Float, val y: Float) {
    /** The smallest scale at which the image covers the frame */
    val minScale get() = max(frameW / imageW, frameH / imageH)

    /** Pinch by [zoom] around [cx], [cy] (frame coordinates) and move by [dx], [dy] */
    fun transform(cx: Float, cy: Float, zoom: Float, dx: Float, dy: Float): CropState {
        val s = (scale * zoom).coerceIn(minScale, minScale * 8)
        val k = s / scale
        return copy(scale = s, x = cx - (cx - x) * k + dx, y = cy - (cy - y) * k + dy).clamped()
    }

    private fun clamped() = copy(
        x = x.coerceIn(min(0f, frameW - imageW * scale), 0f),
        y = y.coerceIn(min(0f, frameH - imageH * scale), 0f),
    )

    /** The part of the image in the frame, in image pixels: left, top, width, height */
    fun source(): IntArray {
        val left = (-x / scale).roundToInt().coerceIn(0, imageW - 1)
        val top = (-y / scale).roundToInt().coerceIn(0, imageH - 1)
        val w = (frameW / scale).roundToInt().coerceIn(1, imageW - left)
        val h = (frameH / scale).roundToInt().coerceIn(1, imageH - top)
        return intArrayOf(left, top, w, h)
    }

    companion object {
        /** The image filling the frame, centred */
        fun initial(imageW: Int, imageH: Int, frameW: Float, frameH: Float): CropState {
            val s = max(frameW / imageW, frameH / imageH)
            return CropState(imageW, imageH, frameW, frameH, s, (frameW - imageW * s) / 2, (frameH - imageH * s) / 2)
        }
    }
}

/** Images are read at most this many pixels on their longer side: enough for any card art */
private const val MAX_IMAGE_SIDE = 2048

/** Read a picked image, scaled down to [MAX_IMAGE_SIDE] and turned upright (from API 28) */
fun loadPickedImage(context: Context, uri: Uri): Bitmap? = try {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
        ImageDecoder.decodeBitmap(ImageDecoder.createSource(context.contentResolver, uri)) { decoder, info, _ ->
            val side = max(info.size.width, info.size.height)
            if (side > MAX_IMAGE_SIDE) {
                val k = MAX_IMAGE_SIDE.toFloat() / side
                decoder.setTargetSize((info.size.width * k).roundToInt().coerceAtLeast(1), (info.size.height * k).roundToInt().coerceAtLeast(1))
            }
            decoder.allocator = ImageDecoder.ALLOCATOR_SOFTWARE
        }
    } else {
        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        context.contentResolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it, null, bounds) }
        var sample = 1
        while (max(bounds.outWidth, bounds.outHeight) / sample > MAX_IMAGE_SIDE) sample *= 2
        context.contentResolver.openInputStream(uri)?.use {
            BitmapFactory.decodeStream(it, null, BitmapFactory.Options().apply { inSampleSize = sample })
        }
    }
} catch (e: Exception) {
    null
}

/** Write the cropped part of [image] as a PNG in [dir] */
fun writeCrop(image: Bitmap, state: CropState, dir: File): File {
    val (left, top, w, h) = state.source()
    val cropped = Bitmap.createBitmap(image, left, top, w, h)
    val file = File(dir, "crop-${UUID.randomUUID()}.png")
    file.outputStream().use { cropped.compress(Bitmap.CompressFormat.PNG, 100, it) }
    if (cropped !== image) cropped.recycle()
    return file
}

/**
 * Full screen: the picked image under a frame of the field's shape ([aspect] = width / height);
 * drag to move it, pinch to zoom. "Use" writes the crop to a file in [cacheDir].
 */
@Composable
fun CropDialog(uri: Uri, aspect: Float, onCropped: (File) -> Unit, onCancel: () -> Unit, cacheDir: File) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var image by remember(uri) { mutableStateOf<Bitmap?>(null) }
    var failed by remember(uri) { mutableStateOf(false) }
    var state by remember(uri) { mutableStateOf<CropState?>(null) }
    LaunchedEffect(uri) {
        image = withContext(Dispatchers.IO) { loadPickedImage(context, uri) }
        failed = image == null
    }
    Dialog(onDismissRequest = onCancel, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxSize()) {
            Column(Modifier.fillMaxSize().safeDrawingPadding().padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Move and zoom the image to fit the frame", style = MaterialTheme.typography.titleMedium)
                BoxWithConstraints(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                    val bitmap = image
                    when {
                        failed -> Text("The image could not be read")
                        bitmap == null -> CircularProgressIndicator()
                        else -> {
                            val safeAspect = if (aspect.isFinite() && aspect > 0f) aspect else 1f
                            val frameW = min(constraints.maxWidth.toFloat(), constraints.maxHeight * safeAspect)
                            val frameH = frameW / safeAspect
                            val s = state?.takeIf { it.frameW == frameW && it.frameH == frameH }
                                ?: CropState.initial(bitmap.width, bitmap.height, frameW, frameH).also { state = it }
                            val shown = remember(bitmap) { bitmap.asImageBitmap() }
                            val frameColor = MaterialTheme.colorScheme.primary
                            Box(
                                Modifier.aspectRatio(safeAspect)
                                    .clipToBounds()
                                    .pointerInput(frameW, frameH) {
                                        detectTransformGestures { centroid, pan, zoom, _ ->
                                            state = state?.transform(centroid.x, centroid.y, zoom, pan.x, pan.y)
                                        }
                                    }
                                    .testTag("crop-frame"),
                            ) {
                                Canvas(Modifier.fillMaxSize()) {
                                    drawImage(
                                        shown,
                                        dstOffset = IntOffset(s.x.roundToInt(), s.y.roundToInt()),
                                        dstSize = IntSize((bitmap.width * s.scale).roundToInt(), (bitmap.height * s.scale).roundToInt()),
                                    )
                                    drawRect(frameColor, style = Stroke(2.dp.toPx()))
                                }
                            }
                        }
                    }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(
                        onClick = {
                            val bitmap = image ?: return@Button
                            val s = state ?: return@Button
                            scope.launch { onCropped(withContext(Dispatchers.IO) { writeCrop(bitmap, s, cacheDir) }) }
                        },
                        enabled = image != null && state != null,
                        modifier = Modifier.testTag("crop-use"),
                    ) { Text("Use") }
                    OutlinedButton(onClick = onCancel) { Text("Cancel") }
                }
            }
        }
    }
}
