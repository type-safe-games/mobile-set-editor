// Mobile Set Editor - the swipe editor's text positions, diffs and regions (#62)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.mobileseteditor.core.FieldRegion

class EditorLogicTest {
    private val emoji = "😀" // one code point, two UTF-16 units

    @Test
    fun codePointsAndUtf16() {
        val s = "a${emoji}b"
        assertEquals(3, CodePoints.toUtf16(s, 2))
        assertEquals(2, CodePoints.fromUtf16(s, 3))
        assertEquals(s.length, CodePoints.toUtf16(s, 99))
    }

    @Test
    fun diffsAreOneReplacementInCodePoints() {
        assertEquals(TextChange(3, 3, "d"), TextDiff.between("abc", "abcd"))
        assertEquals(TextChange(1, 2, ""), TextDiff.between("abc", "ac"))
        assertEquals(TextChange(1, 2, "XY"), TextDiff.between("abc", "aXYc"))
        // after an emoji, positions count it once
        assertEquals(TextChange(2, 2, "!"), TextDiff.between("a$emoji", "a$emoji!"))
        // repeated letters: the cursor says where the typing happened
        assertEquals(TextChange(1, 1, "a"), TextDiff.between("aa", "aaa", cursor = 2))
        assertEquals(TextChange(2, 2, "a"), TextDiff.between("aa", "aaa", cursor = 3))
        assertNull(TextDiff.between("same", "same"))
    }

    @Test
    fun typingAfterAnAutoReplaceMovesWithTheCoresCaret() {
        val sync = TextSync()
        sync.answered("until ", 6)
        // "EOT" typed: the core made it "end of turn", while " y" was typed after it
        sync.answered("until end of turn", 17, shownText = "until EOT", shownCaret = 9)
        val typed = TextDiff.between(sync.shown, "until EOT y", cursor = 11)!!
        assertEquals(TextChange(9, 9, " y"), typed)
        assertEquals(TextChange(17, 17, " y"), sync.toCore(typed))
        // a change before the caret stays where it is
        assertEquals(TextChange(0, 1, "U"), sync.toCore(TextChange(0, 1, "U")))
    }

    @Test
    fun typingSplitsIntoKeystrokes() {
        assertEquals(listOf("E", "O", "T", " "), Keystrokes.of("EOT "))
        assertEquals(listOf("a", emoji), Keystrokes.of("a$emoji"))
        assertEquals(listOf(""), Keystrokes.of(""))
        val paste = "x".repeat(Keystrokes.MAX_SPLIT + 1)
        assertEquals(listOf(paste), Keystrokes.of(paste))
    }

    @Test
    fun positionsSnapOutOfLockedSpans() {
        val locked = listOf(4 until 10)
        assertEquals(4, LockedSpans.snap(5, locked))
        assertEquals(10, LockedSpans.snap(9, locked))
        assertEquals(4, LockedSpans.snap(4, locked))
        assertEquals(2, LockedSpans.snap(2, locked))
        // backspace at the end of a locked span deletes all of it; typing inside one goes beside it
        assertEquals(TextChange(4, 10, ""), LockedSpans.adjust(TextChange(9, 10, ""), locked))
        assertEquals(TextChange(2, 10, ""), LockedSpans.adjust(TextChange(2, 6, ""), locked))
        assertEquals(TextChange(10, 10, "x"), LockedSpans.adjust(TextChange(8, 8, "x"), locked))
        assertEquals(TextChange(1, 2, "x"), LockedSpans.adjust(TextChange(1, 2, "x"), locked))
    }

    private fun region(name: String, x: Double, y: Double, w: Double, h: Double, z: Int, rotation: Double = 0.0, editable: Boolean = true) =
        FieldRegion(name, x, y, w, h, rotation, z, editable)

    @Test
    fun boundsOfRotatedRegions() {
        // a box 40 wide and 10 high, turned 90 degrees counter clockwise around its top-left: on
        // screen it runs upwards from (50, 50)
        val r = region("side", 50.0, 50.0, 40.0, 10.0, 1, rotation = 90.0)
        assertEquals(listOf(50, 10, 60, 50), HitTest.bounds(r).map { Math.round(it).toInt() })
        val flat = region("name", 10.0, 20.0, 30.0, 5.0, 1)
        assertEquals(listOf(10, 20, 40, 25), HitTest.bounds(flat).map { Math.round(it).toInt() })
    }
}
