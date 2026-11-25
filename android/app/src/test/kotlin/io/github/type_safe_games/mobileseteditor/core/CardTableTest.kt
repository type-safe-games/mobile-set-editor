// Mobile Set Editor - the card list table's flat form from the JNI (#86)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class CardTableTest {
    @Test
    fun columnsThenRows() {
        val t = CardTable.parse(arrayOf("2", "name", "Name", "150", "casting cost", "Cost", "50", "Bear", "1G", "Bolt", "R"))!!
        assertEquals(listOf(CardTable.Column("name", "Name", 150), CardTable.Column("casting cost", "Cost", 50)), t.columns)
        assertEquals(listOf(listOf("Bear", "1G"), listOf("Bolt", "R")), t.rows)
    }

    @Test
    fun aFailedCallIsNull() {
        assertNull(CardTable.parse(emptyArray()))
    }
}
