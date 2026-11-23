// Mobile Set Editor - the About screen and the licences screen (#76)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performScrollToNode
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AboutTest {
    @get:Rule
    val compose = createComposeRule()

    private val context get() = InstrumentationRegistry.getInstrumentation().targetContext

    @Test
    fun nameVersionLicenceSourceAndCredits() {
        compose.setContent { AboutScreen(onBack = {}) }
        compose.onNodeWithText("Mobile Set Editor").assertIsDisplayed()
        val version = context.packageManager.getPackageInfo(context.packageName, 0).versionName
        compose.onNodeWithTag("about-version").assertIsDisplayed()
        compose.onNodeWithText("Version $version").assertIsDisplayed()
        compose.onNodeWithText("GNU General Public License", substring = true).assertIsDisplayed()
        compose.onNodeWithText("github.com/type-safe-games/mobile-set-editor", substring = true).performScrollTo().assertIsDisplayed()
        for (who in listOf("twanvl", "haganbmj", "GenevensiS")) compose.onNodeWithText(who, substring = true).performScrollTo()
        compose.onNodeWithText("Not affiliated with", substring = true).performScrollTo().assertIsDisplayed()
    }

    @Test
    fun everyComponentWithItsLicenceText() {
        val (components, libraries) = readLicences(context)
        // the app itself, the native libraries (the build checks that every third_party submodule
        // in the APK is here) and the Java libraries
        val names = components.map { it.name }
        assertEquals("Mobile Set Editor", names.first())
        for (native in listOf("Skia", "zlib", "libpng", "libjpeg-turbo", "FreeType", "HarfBuzz", "Boost", "Hunspell", "minizip-ng")) {
            assertTrue("$native is listed", native in names)
        }
        assertTrue(libraries.any { it.startsWith("androidx.compose.material3:material3:") })
        assertTrue(libraries.any { it.startsWith("org.jetbrains.kotlin:kotlin-stdlib:") })
        // every licence file is there and not empty
        for (c in components) for (f in c.files) {
            assertTrue("${c.name}: $f", context.assets.open("${c.dir}/$f").bufferedReader().readText().isNotBlank())
        }

        compose.setContent { AboutScreen(onBack = {}) }
        compose.onNodeWithTag("about-licences").performScrollTo().performClick()
        compose.onNodeWithTag("licences").performScrollToNode(hasTestTag("licence-FreeType"))
        compose.onNodeWithTag("licence-FreeType").performClick()
        compose.onNodeWithText("The FreeType Project LICENSE", substring = true).assertExists()
    }
}
