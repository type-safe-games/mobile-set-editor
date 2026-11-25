// Mobile Set Editor - what removing installed Templates takes with it (#96)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.core

import java.io.File

/**
 * Removing Templates: what the user chose, what can't work without it (removing a Game takes its
 * Styles; a Style other Styles build on takes them), and the supporting files nothing left needs
 * (includes, symbol fonts, Games). Export templates and locales are only removed when chosen or
 * when they need something removed: nothing references them, so they would always look unused.
 */
object TemplateRemoval {
    data class Plan(val chosen: Set<String>, val dependents: Set<String>, val orphans: Set<String>) {
        val all: Set<String> get() = chosen + dependents + orphans
    }

    private fun supporting(name: String) = name.endsWith(".mse-include") || name.endsWith(".mse-symbol-font") || name.endsWith(".mse-game")

    /** [refs]: each installed package's references to other packages (see [references]) */
    fun plan(refs: Map<String, Set<String>>, chosen: Set<String>): Plan {
        val removed = chosen.filter { it in refs }.toMutableSet()
        // what needs something removed goes too, transitively
        do {
            val more = refs.filter { (name, r) -> name !in removed && r.any { it in removed } }.keys
            removed += more
        } while (more.isNotEmpty())
        val dependents = removed - chosen
        // then the supporting files nothing left references, transitively
        val orphans = mutableSetOf<String>()
        do {
            val left = refs.keys - removed - orphans
            val more = left.filter { n -> supporting(n) && left.none { m -> m != n && n in refs.getValue(m) } }
            orphans += more
        } while (more.isNotEmpty())
        return Plan(chosen.filter { it in refs }.toSet(), dependents, orphans)
    }

    private val PACKAGE = Regex("""[A-Za-z0-9_.\-]+\.mse-(?:game|style|include|symbol-font|export-template|locale)\b""")
    private val BINARY = setOf("png", "jpg", "jpeg", "gif", "bmp", "ttf", "otf", "mse-symbol", "zip")

    /**
     * What each installed package in [store] references: packages it names (in `depends on`, or by
     * path, "/magic-modules.mse-include/..."), and symbol fonts by their bare name, as Styles name
     * them (`symbol font: name: magic-mana-small`). As tools/pack/thin.py finds them.
     */
    fun references(store: File): Map<String, Set<String>> {
        val packages = store.listFiles { f -> f.isDirectory && f.name.contains(".mse-") }.orEmpty().associateBy { it.name }
        val fonts = packages.keys.filter { it.endsWith(".mse-symbol-font") }.map { it.removeSuffix(".mse-symbol-font") }.sortedByDescending { it.length }
        val bare = if (fonts.isEmpty()) null
        else Regex("""(?<![\w.-])(""" + fonts.joinToString("|") { Regex.escape(it) } + """)(?![\w.-])""")
        return packages.mapValues { (name, dir) ->
            val found = mutableSetOf<String>()
            dir.walkTopDown().filter { it.isFile && it.extension.lowercase() !in BINARY }.forEach { f ->
                val text = runCatching { f.readText() }.getOrDefault("")
                PACKAGE.findAll(text).forEach { found += it.value }
                bare?.findAll(text)?.forEach { found += it.groupValues[1] + ".mse-symbol-font" }
            }
            found.filter { it != name && it in packages }.toSet()
        }
    }
}
