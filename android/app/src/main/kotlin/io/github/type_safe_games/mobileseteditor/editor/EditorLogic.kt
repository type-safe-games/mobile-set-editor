// Mobile Set Editor - the swipe editor's logic that needs no Android: text positions, diffs, regions
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import kotlin.math.cos
import kotlin.math.sin
import io.github.type_safe_games.mobileseteditor.core.FieldRegion

/** Code point positions (the core's) to and from UTF-16 indices (Compose's, Java's) */
object CodePoints {
    fun toUtf16(text: String, codePoint: Int): Int {
        val cp = codePoint.coerceIn(0, text.codePointCount(0, text.length))
        return text.offsetByCodePoints(0, cp)
    }

    fun fromUtf16(text: String, index: Int): Int = text.codePointCount(0, index.coerceIn(0, text.length))
}

/** One edit as the core takes it: replace [start, end) (code points) of the old text with [text] */
data class TextChange(val start: Int, val end: Int, val text: String)

object TextDiff {
    /**
     * The single replacement that turns [old] into [new], in code points, as one keystroke, a paste
     * or a deletion makes. [cursor] (UTF-16, in [new], after the edit) disambiguates repeated
     * characters: the changed text ends at the cursor, so the common suffix doesn't reach before it.
     */
    fun between(old: String, new: String, cursor: Int? = null): TextChange? {
        if (old == new) return null
        val shorter = minOf(old.length, new.length)
        val maxSuffix = if (cursor == null) shorter else minOf(shorter, new.length - cursor.coerceIn(0, new.length))
        var suffix = 0
        while (suffix < maxSuffix && old[old.length - 1 - suffix] == new[new.length - 1 - suffix]) suffix++
        var prefix = 0
        while (prefix < shorter - suffix && old[prefix] == new[prefix]) prefix++
        // don't split a surrogate pair
        if (prefix > 0 && Character.isHighSurrogate(old[prefix - 1]) && prefix < old.length) prefix--
        if (suffix > 0 && Character.isLowSurrogate(old[old.length - suffix]) && old.length - suffix > prefix) suffix--
        val oldEnd = old.length - suffix
        val newEnd = new.length - suffix
        return TextChange(
            start = CodePoints.fromUtf16(old, prefix),
            end = CodePoints.fromUtf16(old, oldEnd),
            text = new.substring(prefix, newEnd),
        )
    }
}

/**
 * The field's text as the editor shows it and as the core holds it. They differ while the core
 * has rewritten what was typed (auto-replace, scripts) and more typing came before that could be
 * shown: further changes are made against the shown text, and moved to where the core's caret is.
 */
class TextSync {
    var core = ""
        private set
    var shown = ""
        private set
    private var coreCaret = 0
    private var shownCaret = 0

    /** The core's text, and the shown text it came from ([shownText] with its caret), in code points */
    fun answered(coreText: String, coreCaret: Int, shownText: String = coreText, shownCaret: Int = coreCaret) {
        core = coreText
        this.coreCaret = coreCaret
        shown = shownText
        this.shownCaret = shownCaret
    }

    /** A change of the shown text, as a change of the core's: positions from the caret on move with it */
    fun toCore(change: TextChange): TextChange {
        val shift = coreCaret - shownCaret
        fun map(p: Int) = if (p >= shownCaret) p + shift else p
        return TextChange(map(change.start), map(change.end), change.text)
    }
}

/**
 * Typing as keystrokes: text typed faster than the core answers arrives as one change, but the
 * core applies auto-replace at the caret after each insertion, as the desktop does per key. So an
 * insertion goes to the core one character (code point) at a time; a long one (a paste) at once.
 */
object Keystrokes {
    const val MAX_SPLIT = 64

    fun of(text: String): List<String> {
        val n = text.codePointCount(0, text.length)
        if (n <= 1 || n > MAX_SPLIT) return listOf(text)
        val out = ArrayList<String>(n)
        var i = 0
        while (i < text.length) {
            val next = text.offsetByCodePoints(i, 1)
            out += text.substring(i, next)
            i = next
        }
        return out
    }
}

object LockedSpans {
    /** A position (code points) moved out of any locked span, to its nearest edge */
    fun snap(position: Int, locked: List<IntRange>): Int {
        for (span in locked) {
            val end = span.last + 1
            if (position > span.first && position < end) {
                return if (position - span.first <= end - position) span.first else end
            }
        }
        return position
    }

    /**
     * A change made atomic over locked spans: a deletion that cuts into one takes all of it, and an
     * insertion inside one moves to its nearest edge.
     */
    fun adjust(change: TextChange, locked: List<IntRange>): TextChange {
        if (change.start == change.end) {
            val at = snap(change.start, locked)
            return change.copy(start = at, end = at)
        }
        var start = change.start
        var end = change.end
        for (span in locked) {
            val spanEnd = span.last + 1
            if (start > span.first && start < spanEnd) start = span.first
            if (end > span.first && end < spanEnd) end = spanEnd
        }
        return change.copy(start = start, end = end)
    }
}

object HitTest {
    /** A region's bounding box on screen (it may be rotated counter clockwise around its top-left corner): left, top, right, bottom */
    fun bounds(r: FieldRegion): DoubleArray {
        val a = Math.toRadians(r.rotation)
        val xs = DoubleArray(4)
        val ys = DoubleArray(4)
        for ((i, corner) in listOf(0.0 to 0.0, r.width to 0.0, 0.0 to r.height, r.width to r.height).withIndex()) {
            val (lx, ly) = corner
            xs[i] = r.x + lx * cos(a) + ly * sin(a)
            ys[i] = r.y - lx * sin(a) + ly * cos(a)
        }
        return doubleArrayOf(xs.min(), ys.min(), xs.max(), ys.max())
    }
}
