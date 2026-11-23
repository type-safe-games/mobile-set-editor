// Mobile Set Editor - which Templates to install: dependencies follow (#71)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ImportChoiceTest {
    private val ok = CoreResult(0, "", "", emptyArray())

    private fun pkg(name: String, group: String, size: Long, verdict: Verdict, vararg deps: String) =
        ScannedPackage(name, name.substringAfterLast('.'), name, "1", "", group, size, verdict.ordinal, "", arrayOf(*deps))

    private val scan = ScanResult(
        ok,
        arrayOf(
            pkg("magic.mse-game", "magic", 100, Verdict.Install),
            pkg("magic-mana.mse-symbol-font", "magic/fonts", 10, Verdict.Install, "magic.mse-game"),
            pkg("magic-m15.mse-style", "magic/m15", 50, Verdict.Install, "magic.mse-game", "magic-mana.mse-symbol-font", "Magic Set Editor"),
            pkg("magic-old.mse-style", "magic/old", 40, Verdict.SkipNotNewer, "magic.mse-game"),
            pkg("broken.mse-style", "other", 5, Verdict.Refuse),
        ),
        3, 30, 2, 20, 0, 0,
    )

    @Test
    fun pickingAPackagePicksWhatItNeeds() {
        val c = ImportChoice.nothing(scan).selectGroup("magic/m15")
        assertEquals(setOf("magic-m15.mse-style", "magic-mana.mse-symbol-font", "magic.mse-game"), c.packages)
        assertEquals(160L, c.size)
        assertEquals(listOf("magic-m15.mse-style", "magic-mana.mse-symbol-font"), c.neededBy("magic.mse-game").sorted())
    }

    @Test
    fun aGroupHasTheGroupsUnderIt() {
        val c = ImportChoice.nothing(scan).selectGroup("magic")
        assertEquals(setOf("magic.mse-game", "magic-mana.mse-symbol-font", "magic-m15.mse-style"), c.packages)
        assertEquals(emptySet<String>(), c.clearGroup("magic").packages)
        assertTrue(!ImportChoice.inGroup("magicx", "magic"))
    }

    @Test
    fun clearingANeededPackageClearsWhatNeedsIt() {
        val c = ImportChoice.nothing(scan).select("magic-m15.mse-style").clear("magic-mana.mse-symbol-font")
        assertEquals(setOf("magic.mse-game"), c.packages)
    }

    @Test
    fun onlyInstallablePackagesArePicked() {
        val c = ImportChoice.nothing(scan).select("broken.mse-style").select("magic-old.mse-style")
        assertTrue(c.packages.isEmpty())
        val all = ImportChoice.everything(scan)
        assertEquals(setOf("magic.mse-game", "magic-mana.mse-symbol-font", "magic-m15.mse-style"), all.packages)
        assertEquals(160L + 20, all.size) // with the new fonts
    }
}
