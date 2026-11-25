// Mobile Set Editor - where a tap on the Card puts the caret, in a combined field (#83)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import io.github.type_safe_games.mobileseteditor.core.Caret
import org.junit.Assert.assertEquals
import org.junit.Test

class NearestCaretTest {
    // rules "ab" on the line at 0..10, the separator, flavor "cd" on the line at 20..30
    private val carets = listOf(
        Caret(0.0, 0.0, 10.0), Caret(10.0, 0.0, 10.0), Caret(20.0, 0.0, 10.0), // a b |
        Caret(0.0, 20.0, 30.0), Caret(10.0, 20.0, 30.0), Caret(20.0, 20.0, 30.0), // c d |
    )
    private val parts = listOf(0 until 2, 3 until 5)

    @Test
    fun aTapOnTheFlavorLineGoesIntoTheFlavor() {
        assertEquals(4, nearestCaret(carets, 11.0, 25.0, parts))
    }

    @Test
    fun aTapBetweenTheLinesStaysInTheRules() {
        // nearer the flavor line, but above it: the end of the rules
        assertEquals(2, nearestCaret(carets, 30.0, 18.0, parts))
    }

    @Test
    fun anEmptyFlavorIsNotEnteredByTapping() {
        val empty = listOf(0 until 2, 3 until 3)
        assertEquals(2, nearestCaret(carets.take(4), 0.0, 25.0, empty))
    }

    @Test
    fun aFieldWithoutPartsTakesTheNearestCaret() {
        assertEquals(3, nearestCaret(carets, 0.0, 25.0))
    }
}
