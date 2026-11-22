// Mobile Set Editor - the render service's renderer over the core
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.render

import android.graphics.Bitmap
import org.mobileseteditor.core.CoreMessage
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.FieldRegion

/** Renders a session's Cards into Bitmaps on the core thread; the core's messages go to [onMessages] */
class CoreCardRenderer(
    private val session: Long,
    private val onMessages: (List<CoreMessage>) -> Unit = {},
) : CardRenderer<Bitmap> {
    override suspend fun render(key: RenderKey, spare: (Int, Int) -> Bitmap?): Pair<Bitmap, List<FieldRegion>>? {
        val size = Core.cardSize(session, key.uid)
        onMessages(size.result.messages.toList())
        if (!size.result.ok || size.width <= 0 || key.width <= 0) return null
        val height = (key.width * size.height / size.width).toInt().coerceAtLeast(1)
        val bitmap = spare(key.width, height) ?: Bitmap.createBitmap(key.width, height, Bitmap.Config.ARGB_8888)
        val r = Core.renderCard(session, key.uid, bitmap)
        onMessages(r.result.messages.toList())
        return if (r.result.ok) bitmap to r.regions.toList() else null
    }

    override fun byteSize(image: Bitmap): Long = image.allocationByteCount.toLong()
    override fun width(image: Bitmap) = image.width
    override fun height(image: Bitmap) = image.height

    companion object {
        /** The cache's budget: an eighth of what the app's heap may grow to (Bitmap pixels live in
         *  native memory since Android 8, but the heap limit says how big the device class is) */
        fun defaultBudget(): Long = Runtime.getRuntime().maxMemory() / 8
    }
}
