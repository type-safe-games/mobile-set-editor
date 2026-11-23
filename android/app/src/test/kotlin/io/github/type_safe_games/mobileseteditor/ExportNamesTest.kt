// Mobile Set Editor - file names for exported Cards (#74)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import org.junit.Assert.assertEquals
import org.junit.Test

class ExportNamesTest {
    @Test
    fun safeAndUsedOnce() {
        val used = mutableSetOf<String>()
        assertEquals("Lightning Bolt.png", ExportNames.next("Lightning Bolt", 1, used))
        assertEquals("Lightning Bolt (2).png", ExportNames.next("Lightning Bolt", 2, used))
        assertEquals("lightning bolt (3).png", ExportNames.next("lightning bolt", 3, used))
        assertEquals("A_B_ C.png", ExportNames.next("A/B: C", 4, used))
        assertEquals("card-5.png", ExportNames.next("  ", 5, used))
        assertEquals("Fire_Ice.png", ExportNames.next("Fire\nIce", 6, used))
    }
}
