// Mobile Set Editor - a combined field's parts, between its separators (#83)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.core

import io.github.type_safe_games.mobileseteditor.editor.partLabel
import org.junit.Assert.assertEquals
import org.junit.Test

class TextPartsTest {
    private val ok = ActionResult(CoreResult(0, "", "", emptyArray()), false, emptyArray(), false, false, false, false, "", "")

    private fun text(t: String, vararg seps: IntRange) =
        TextResult(ok, t, 0, 0, IntArray(0), IntArray(0), seps.map { it.first }.toIntArray(), seps.map { it.last + 1 }.toIntArray())

    @Test
    fun rulesThenFlavor() {
        // "Draw a card." + separator "\n" + "Flavor": rules 0..12, flavor 13..19
        assertEquals(listOf(0 until 12, 13 until 19), text("Draw a card.\nFlavor", 12 until 13).parts)
    }

    @Test
    fun anEmptyFlavorStillHasAPlaceAfterTheSeparator() {
        assertEquals(listOf(0 until 12, 13 until 13), text("Draw a card.\n", 12 until 13).parts)
    }

    @Test
    fun noSeparatorIsOnePart() {
        assertEquals(listOf(0 until 4), text("Bear").parts)
    }

    @Test
    fun labels() {
        assertEquals("Flavor", partLabel("text", 1, 2))
        assertEquals("Part 2", partLabel("other", 1, 2))
    }
}
