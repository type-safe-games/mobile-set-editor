// Mobile Set Editor - Magic's symbol menus, with the symbols a field most likely needs first (#82)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import io.github.type_safe_games.mobileseteditor.core.SymbolMenuItem

/**
 * Magic's mana fonts list their symbols in their own order: the casting cost font starts with tap,
 * untap, energy, ticket and acorn, which no printed mana cost has. The bar scrolls sideways, so the
 * order decides what a cost needs a search for. This puts them in three groups, by symbol code:
 *
 * 1. Plain mana: a generic number (the font's "custom" entry, or digits), X, Y, Z, colorless C, the
 *    colors W U B R G, and snow S. In rules text, tap and untap come before these.
 * 2. Combinations: hybrid, colorless hybrid, Phyrexian, three-color and 2/color hybrids, whether in
 *    a submenu or not.
 * 3. Everything else, in the font's own order: energy, tickets, acorns, half mana, infinity, a
 *    Style's custom symbols, ...
 *
 * Within the last two, the font's order stays. The font's dividers are dropped; one goes between
 * groups. Other Games' menus are left as their fonts have them.
 */
object SymbolOrder {
    /** Codes a mana symbol is made of, joined with "/" in a combination */
    private val manaParts = setOf("W", "U", "B", "R", "G", "C", "S", "H", "2")
    private val plain = listOf("X", "Y", "Z", "C", "W", "U", "B", "R", "G", "S")
    private val rulesFirst = listOf("T", "Q")

    fun forField(game: String, items: List<SymbolMenuItem>, castingCost: Boolean): List<SymbolMenuItem> {
        if (game != "magic") return items
        val entries = items.filter { it.type != SymbolMenuItem.Type.Line }
        val first = if (castingCost) emptyList() else entries.filter { it.type == SymbolMenuItem.Type.Code && it.code in rulesFirst }
            .sortedBy { rulesFirst.indexOf(it.code) }
        val rest = entries - first.toSet()
        val basic = rest.filter { rank(it) != null }.sortedBy { rank(it) }
        val combined = rest.filter { rank(it) == null && isCombination(it) }
        val others = rest - basic.toSet() - combined.toSet()
        val groups = listOf(first + basic, combined, others).filter { it.isNotEmpty() }
        return groups.flatMapIndexed { i, group -> if (i == 0) group else listOf(divider) + group }
    }

    /** A plain mana entry's place in the first group, null for anything else */
    private fun rank(item: SymbolMenuItem): Int? = when (item.type) {
        // a font's "custom" entry asks for a number: generic mana
        SymbolMenuItem.Type.Custom -> 0
        SymbolMenuItem.Type.Code -> when {
            item.code.isNotEmpty() && item.code.all { it.isDigit() } -> 1
            item.code in plain -> 2 + plain.indexOf(item.code)
            else -> null
        }
        else -> null
    }

    /** A combination of mana symbols (W/U, C/G, H/R, 2/W, W/U/B...), or a submenu that is mostly those */
    private fun isCombination(item: SymbolMenuItem): Boolean = when (item.type) {
        SymbolMenuItem.Type.Code -> isCombinedCode(item.code)
        SymbolMenuItem.Type.Submenu -> {
            val codes = item.items.filter { it.type == SymbolMenuItem.Type.Code }.map { it.code }
            codes.isNotEmpty() && codes.count(::isCombinedCode) * 2 > codes.size
        }
        else -> false
    }

    /** Phyrexian mana is "H" alone or "H/W"; hybrids are two or three parts */
    private fun isCombinedCode(code: String): Boolean =
        code == "H" || code.split('/').let { parts -> parts.size >= 2 && parts.all { it in manaParts } }

    private val divider = SymbolMenuItem(SymbolMenuItem.Type.Line, "", "", "", -1, emptyList())
}
