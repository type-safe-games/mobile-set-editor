// Mobile Set Editor - the field sheet's value editors: crops, multiple choices, colors (#63)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import androidx.compose.ui.graphics.Color
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ValueEditorLogicTest {
    @Test
    fun aCropStartsCentredAndCoveringTheFrame() {
        // a 400x200 image in a 100x100 frame: scaled to 200x100, the middle square shown
        val s = CropState.initial(400, 200, 100f, 100f)
        assertEquals(0.5f, s.scale)
        assertArrayEquals(intArrayOf(100, 0, 200, 200), s.source())
    }

    @Test
    fun panningAndZoomingStayInsideTheImage() {
        val s = CropState.initial(400, 200, 100f, 100f)
        // dragging far right shows the left edge, no further
        assertArrayEquals(intArrayOf(0, 0, 200, 200), s.transform(50f, 50f, 1f, 1000f, 0f).source())
        // zooming in 2x around the centre: the middle 100x100
        assertArrayEquals(intArrayOf(150, 50, 100, 100), s.transform(50f, 50f, 2f, 0f, 0f).source())
        // zooming out further than covering is not allowed
        assertEquals(0.5f, s.transform(50f, 50f, 0.1f, 0f, 0f).scale)
    }

    @Test
    fun multipleChoicesKeepTheFieldsOrder() {
        val choices = listOf("white", "blue", "black", "red", "green")
        assertEquals("white, red", MultipleChoice.toggle(choices, "red", "white"))
        assertEquals("red", MultipleChoice.toggle(choices, "white, red", "white"))
        assertEquals("blue", MultipleChoice.toggle(choices, "", "blue"))
        assertEquals(setOf("a", "b"), MultipleChoice.parse("a, b"))
    }

    @Test
    fun colorsAsTheCoreWritesThem() {
        assertEquals(Color(255, 0, 0), CoreColor.parse("rgb(255,0,0)"))
        assertEquals(Color(0, 0, 255, 128), CoreColor.parse("rgba(0, 0, 255, 128)"))
        assertEquals(Color(0x12, 0x34, 0x56), CoreColor.parse("#123456"))
        assertNull(CoreColor.parse("nope"))
        assertEquals("rgb(1,2,3)", CoreColor.format(1, 2, 3))
    }
}
