// Mobile Set Editor - what removing Templates takes with it (#96)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.core

import java.io.File
import java.nio.file.Files
import org.junit.Assert.assertEquals
import org.junit.Test

class TemplateRemovalTest {
    private val refs = mapOf(
        "magic.mse-game" to emptySet(),
        "magic-mana.mse-symbol-font" to setOf("magic.mse-game"),
        "magic-modules.mse-include" to setOf("magic.mse-game"),
        "magic-m15.mse-style" to setOf("magic.mse-game", "magic-mana.mse-symbol-font", "magic-modules.mse-include"),
        "magic-m15-saga.mse-style" to setOf("magic.mse-game", "magic-m15.mse-style", "magic-mana.mse-symbol-font"),
        "magic-old.mse-style" to setOf("magic.mse-game", "magic-mana.mse-symbol-font"),
        "magic-spoiler.mse-export-template" to setOf("magic.mse-game"),
    )

    @Test
    fun aStyleTakesWhatBuildsOnItAndWhatNothingElseNeeds() {
        val plan = TemplateRemoval.plan(refs, setOf("magic-m15.mse-style"))
        assertEquals(setOf("magic-m15.mse-style"), plan.chosen)
        assertEquals(setOf("magic-m15-saga.mse-style"), plan.dependents)
        // the modules were only the M15 Styles'; the mana font and the Game are still used
        assertEquals(setOf("magic-modules.mse-include"), plan.orphans)
    }

    @Test
    fun theGameTakesEverythingThatNeedsIt() {
        val plan = TemplateRemoval.plan(refs, setOf("magic.mse-game"))
        assertEquals(refs.keys, plan.all)
    }

    @Test
    fun referencesAreFoundByNameAndSymbolFontsByBareName() {
        val store = Files.createTempDirectory("store").toFile()
        fun pkg(name: String, file: String, text: String) = File(store, name).apply { mkdirs() }.resolve(file).writeText(text)
        pkg("magic.mse-game", "game", "mse version: 2.0.0\n")
        pkg("magic-mana-small.mse-symbol-font", "symbol-font", "")
        pkg("magic-modules.mse-include", "include", "")
        pkg("magic-m15.mse-style", "style", "depends on:\n\tpackage: magic.mse-game\nsymbol font:\n\tname: magic-mana-small\nimage: /magic-modules.mse-include/x.png\n")
        val refs = TemplateRemoval.references(store)
        assertEquals(setOf("magic.mse-game", "magic-mana-small.mse-symbol-font", "magic-modules.mse-include"), refs.getValue("magic-m15.mse-style"))
        assertEquals(emptySet<String>(), refs.getValue("magic.mse-game"))
        store.deleteRecursively()
    }
}
