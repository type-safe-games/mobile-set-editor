// Mobile Set Editor - Magic's symbol menus put plain mana first, then combinations, then the rest (#82)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import io.github.type_safe_games.mobileseteditor.core.SymbolMenuItem
import io.github.type_safe_games.mobileseteditor.core.SymbolMenuItem.Type
import org.junit.Assert.assertEquals
import org.junit.Test

class SymbolOrderTest {
    private fun code(c: String) = SymbolMenuItem(Type.Code, c, c, "", 0, emptyList())
    private val line = SymbolMenuItem(Type.Line, "", "", "", -1, emptyList())
    private fun custom(name: String) = SymbolMenuItem(Type.Custom, name, "", "Number", 0, emptyList())
    private fun menu(name: String, vararg codes: String) = SymbolMenuItem(Type.Submenu, name, "", "", -1, codes.map(::code))

    /** What the bar shows: a code, a submenu's or custom entry's label, "|" for a divider */
    private fun shown(items: List<SymbolMenuItem>) = items.map {
        when (it.type) { Type.Code -> it.code; Type.Line -> "|"; else -> it.label }
    }

    // magic-mana-large's menu (the casting cost font of the M15 Styles), shortened
    private val large = listOf(
        code("T"), code("Q"), code("E"), code("TK"), code("A"), line,
        code("X"), code("Y"), code("Z"), code("I"), code("C"), custom("generic"), line,
        code("W"), code("U"), code("B"), code("R"), code("G"), code("S"), line,
        menu("half", "1/2", "|W", "|U"),
        menu("hybrid", "W/U", "U/B"),
        menu("colorless hybrid", "C/W", "C/U"),
        menu("phyrexian", "H", "H/W"),
        menu("hybrid 3", "W/U/B", "U/B/R"), line,
        menu("number hybrids", "2/W", "2/U"),
    )

    @Test
    fun aCastingCostStartsWithNumbersXColorlessAndColorsAndEndsWithTheOddsAndEnds() {
        assertEquals(
            listOf(
                "generic", "X", "Y", "Z", "C", "W", "U", "B", "R", "G", "S", "|",
                "hybrid", "colorless hybrid", "phyrexian", "hybrid 3", "number hybrids", "|",
                "T", "Q", "E", "TK", "A", "I", "half",
            ),
            shown(SymbolOrder.forField("magic", large, castingCost = true)),
        )
    }

    @Test
    fun rulesTextPutsTapAndUntapFirst() {
        val small = listOf(
            custom("colorless"), code("∞"), line, code("W"), code("U"), code("C"), line,
            menu("hybrid", "W/U", "C/W"), code("W/U/B/R/G"), line,
            menu("half mana", "|1", "|W"), menu("custom symbols", "V", "V/W"), code("E"), code("Q"), code("T"),
        )
        assertEquals(
            listOf("T", "Q", "colorless", "C", "W", "U", "|", "hybrid", "W/U/B/R/G", "|", "∞", "half mana", "custom symbols", "E"),
            shown(SymbolOrder.forField("magic", small, castingCost = false)),
        )
    }

    @Test
    fun otherGamesKeepTheirFontsOrder() {
        assertEquals(large, SymbolOrder.forField("vs", large, castingCost = true))
    }
}
